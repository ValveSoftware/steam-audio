use crate::geometry::{Aabb, Ray};
use crate::math::{Vector2, Vector3, cpp_max, cpp_min};

use super::mesh::{Hit, Mesh};
use super::msvc_sort;

/// What a [`BvhNode`] is.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BvhNodeKind {
    /// A single triangle of the mesh.
    Leaf { triangle: usize },
    /// An internal node whose children are at `left_child` and `left_child + 1` (absolute node indices), split
    /// along `split_axis` (0 = x, 1 = y, 2 = z).
    Internal {
        left_child: usize,
        split_axis: usize,
    },
}

/// A node of a [`Bvh`] (upstream `BVHNode`): a bounding box and, packed as upstream packs it into the box's
/// padding, either a triangle index (leaf) or the offset to the left child and the split axis.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct BvhNode {
    pub bounds: Aabb,
    /// Leaf: `triangle << 2 | 3`; internal: `left_child_offset << 2 | split_axis`.
    data: i32,
}

impl Default for BvhNode {
    fn default() -> Self {
        Self {
            bounds: Aabb::new(Vector3::ZERO, Vector3::ZERO),
            data: 0,
        }
    }
}

impl BvhNode {
    pub fn is_leaf(&self) -> bool {
        self.data & 3 == 3
    }

    /// The node's packed data word, exactly as upstream stores it.
    pub fn packed_data(&self) -> i32 {
        self.data
    }

    /// Decodes the node; `index` is this node's own index in [`Bvh::nodes`].
    pub fn kind(&self, index: usize) -> BvhNodeKind {
        if self.is_leaf() {
            BvhNodeKind::Leaf {
                triangle: self.payload(),
            }
        } else {
            BvhNodeKind::Internal {
                left_child: index + self.payload(),
                split_axis: self.split_axis(),
            }
        }
    }

    fn split_axis(&self) -> usize {
        (self.data & 3) as usize
    }

    /// Triangle index (leaf) or left child offset (internal).
    fn payload(&self) -> usize {
        (self.data >> 2) as usize
    }
}

/// A bounding volume hierarchy over a mesh's triangles (upstream `BVH`), built with the surface area heuristic,
/// falling back to an object median split for degenerate nodes.
///
/// Leaves hold one triangle each, so a mesh of `n` triangles has `2n - 1` nodes, laid out exactly as upstream
/// lays them out: the root at 0, and the children of every internal node next to each other.
#[derive(Debug, Clone, PartialEq)]
pub struct Bvh {
    nodes: Vec<BvhNode>,
}

/// A box grown with SSE `min`/`max` (upstream `GrowableBox`), which return the second operand when either is NaN.
#[derive(Debug, Clone, Copy)]
struct GrowableBox {
    min: [f32; 3],
    max: [f32; 3],
}

#[inline]
fn sse_min(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

#[inline]
fn sse_max(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

impl GrowableBox {
    const EMPTY: Self = Self {
        min: [f32::MAX; 3],
        max: [-f32::MAX; 3],
    };

    fn to_aabb(self) -> Aabb {
        Aabb::new(Vector3::from_array(self.min), Vector3::from_array(self.max))
    }

    fn grow_to_contain_point(&mut self, point: Vector3) {
        self.grow_to_contain(&Self {
            min: point.to_array(),
            max: point.to_array(),
        });
    }

    fn grow_to_contain(&mut self, other: &Self) {
        for (min, &other) in self.min.iter_mut().zip(&other.min) {
            *min = sse_min(*min, other);
        }
        for (max, &other) in self.max.iter_mut().zip(&other.max) {
            *max = sse_max(*max, other);
        }
    }

    /// Upstream's SSE shuffles compute `2 * (dx*dy + (dz*dx + dy*dz))`, which is not the association order of
    /// `Aabb::surface_area`.
    fn surface_area(&self) -> f32 {
        let dx = self.max[0] - self.min[0];
        let dy = self.max[1] - self.min[1];
        let dz = self.max[2] - self.min[2];
        (dx * dy + (dz * dx + dy * dz)) * 2.0
    }
}

/// One coordinate of a leaf's centroid (upstream `CentroidCoordinate`).
#[derive(Debug, Clone, Copy)]
struct Centroid {
    coordinate: f32,
    leaf: u32,
}

/// Leaves `[start, end]` (inclusive) of the node at `output`, whose children go to `left_child` and
/// `left_child + 1` (upstream `ConstructionTask`).
#[derive(Debug, Clone, Copy)]
struct ConstructionTask {
    output: usize,
    start: usize,
    end: usize,
    left_child: usize,
}

/// Upstream `Split`: the first `index` sorted leaves go left.
#[derive(Debug, Clone, Copy)]
struct Split {
    index: usize,
    axis: usize,
}

/// Upstream `TraversalTask`.
#[derive(Debug, Clone, Copy, Default)]
struct TraversalTask {
    node: usize,
    t_min: f32,
    t_max: f32,
}

/// Upstream's traversal stacks hold 128 entries (and overflow on deeper trees); this one spills to the heap
/// instead, so traversal never allocates for trees of practical depth.
struct TraversalStack<T> {
    inline: [T; 128],
    len: usize,
    spill: Vec<T>,
}

impl<T: Copy + Default> TraversalStack<T> {
    fn new() -> Self {
        Self {
            inline: [T::default(); 128],
            len: 0,
            spill: Vec::new(),
        }
    }

    fn push(&mut self, value: T) {
        if self.len < self.inline.len() {
            self.inline[self.len] = value;
            self.len += 1;
        } else {
            self.spill.push(value);
        }
    }

    fn pop(&mut self) -> Option<T> {
        if let Some(value) = self.spill.pop() {
            return Some(value);
        }
        if self.len == 0 {
            return None;
        }
        self.len -= 1;
        Some(self.inline[self.len])
    }
}

/// Reciprocal direction and per-axis direction signs, as upstream computes them for traversal. Zero components
/// (of either sign) get a reciprocal of `+∞`.
fn ray_setup(ray: &Ray) -> (Vector3, [usize; 3]) {
    let d = ray.direction.to_array();
    let reciprocal = d.map(|x| if x == 0.0 { f32::INFINITY } else { 1.0 / x });
    let signs = d.map(|x| usize::from(x >= 0.0));
    (Vector3::from_array(reciprocal), signs)
}

impl Bvh {
    /// Builds the hierarchy (upstream `BVH(mesh)`).
    ///
    /// # Panics
    ///
    /// If the mesh has no triangles (upstream allocates `2 * 0 - 1` nodes), or `2^29` or more.
    pub fn new(mesh: &Mesh) -> Self {
        let num_triangles = mesh.num_triangles();
        assert!(num_triangles > 0, "a BVH needs at least one triangle");
        assert!(
            num_triangles < 1 << 29,
            "too many triangles for a BVH: {num_triangles}"
        );

        let mut bvh = Self {
            nodes: vec![BvhNode::default(); 2 * num_triangles - 1],
        };

        let mut leaf_indices: Vec<u32> = (0..num_triangles as u32).collect();

        let leaf_boxes: Vec<GrowableBox> = (0..num_triangles)
            .map(|i| {
                let mut b = GrowableBox::EMPTY;
                for v in mesh.triangle_vertices(i) {
                    b.grow_to_contain_point(v);
                }
                b
            })
            .collect();

        let leaf_centers: Vec<Vector3> = leaf_boxes.iter().map(|b| b.to_aabb().center()).collect();

        let empty = Centroid {
            coordinate: 0.0,
            leaf: 0,
        };
        let mut centroids = [
            vec![empty; num_triangles],
            vec![empty; num_triangles],
            vec![empty; num_triangles],
        ];
        let mut surface_areas = vec![0.0; num_triangles];

        let mut stack = Vec::new();
        let mut task = ConstructionTask {
            output: 0,
            start: 0,
            end: num_triangles - 1,
            left_child: 1,
        };

        loop {
            if task.start == task.end {
                let leaf = leaf_indices[task.start] as usize;
                bvh.nodes[task.output] = BvhNode {
                    bounds: leaf_boxes[leaf].to_aabb(),
                    data: ((leaf as i32) << 2) | 3,
                };

                match stack.pop() {
                    Some(next) => task = next,
                    None => break,
                }
            } else {
                let mut bounds = GrowableBox::EMPTY;
                for &leaf in &leaf_indices[task.start..=task.end] {
                    bounds.grow_to_contain(&leaf_boxes[leaf as usize]);
                }
                bvh.nodes[task.output].bounds = bounds.to_aabb();

                for i in task.start..=task.end {
                    let leaf = leaf_indices[i];
                    let center = leaf_centers[leaf as usize].to_array();
                    for (axis, centroids) in centroids.iter_mut().enumerate() {
                        centroids[i] = Centroid {
                            coordinate: center[axis],
                            leaf,
                        };
                    }
                }

                let split = best_split(
                    &leaf_boxes,
                    &mut leaf_indices,
                    &mut centroids,
                    &mut surface_areas,
                    &bounds,
                    task.start,
                    task.end,
                );
                let offset = (task.left_child - task.output) as i32;
                bvh.nodes[task.output].data = (offset << 2) | split.axis as i32;

                stack.push(ConstructionTask {
                    output: task.left_child + 1,
                    start: task.start + split.index,
                    end: task.end,
                    left_child: task.left_child + 2 * split.index,
                });
                task = ConstructionTask {
                    output: task.left_child,
                    start: task.start,
                    end: task.start + split.index - 1,
                    left_child: task.left_child + 2,
                };
            }
        }

        bvh
    }

    pub fn num_nodes(&self) -> usize {
        self.nodes.len()
    }

    pub fn nodes(&self) -> &[BvhNode] {
        &self.nodes
    }

    pub fn node(&self, index: usize) -> &BvhNode {
        &self.nodes[index]
    }

    /// Bounding box of the whole mesh.
    pub fn bounds(&self) -> Aabb {
        self.nodes[0].bounds
    }

    /// The closest hit at a distance in `[min_distance, max_distance]` along the ray (upstream `intersect`). Only
    /// `distance` and `triangle_index` are set.
    ///
    /// A triangle is accepted if `min_distance <= t < (closest so far)`, so `max_distance` only prunes boxes: a
    /// triangle just beyond it can still be reported when its box reaches inside the range (upstream behaviour).
    pub fn closest_hit(&self, ray: &Ray, mesh: &Mesh, min_distance: f32, max_distance: f32) -> Hit {
        let mut hit = Hit::default();
        let (reciprocal, signs) = ray_setup(ray);

        let mut stack = TraversalStack::<TraversalTask>::new();
        let mut task = TraversalTask {
            node: 0,
            t_min: min_distance,
            t_max: max_distance,
        };

        loop {
            let node = &self.nodes[task.node];

            if ray.intersect_aabb(
                &node.bounds,
                reciprocal,
                signs,
                &mut task.t_min,
                &mut task.t_max,
            ) {
                if node.is_leaf() {
                    let triangle = node.payload();
                    let t = mesh.intersect(ray, triangle);
                    if min_distance <= t && t < hit.distance {
                        hit.distance = t;
                        hit.triangle_index = Some(triangle);
                    }
                } else {
                    let offset = node.payload();
                    let sign = signs[node.split_axis()];
                    stack.push(TraversalTask {
                        node: task.node + offset + sign,
                        ..task
                    });
                    task.node += offset + (sign ^ 1);
                    continue;
                }
            }

            match stack.pop() {
                Some(next) => {
                    task = next;
                    task.t_max = cpp_min(task.t_max, hit.distance);
                }
                None => break,
            }
        }

        hit
    }

    /// `true` if any triangle is hit at a distance in `[min_distance, max_distance)` (upstream `isOccluded`).
    pub fn any_hit(&self, ray: &Ray, mesh: &Mesh, min_distance: f32, max_distance: f32) -> bool {
        let (reciprocal, signs) = ray_setup(ray);

        let mut stack = TraversalStack::<usize>::new();
        let mut node_index = 0;

        loop {
            let node = &self.nodes[node_index];
            let mut t_min = min_distance;
            let mut t_max = max_distance;

            if ray.intersect_aabb(&node.bounds, reciprocal, signs, &mut t_min, &mut t_max) {
                if node.is_leaf() {
                    let t = mesh.intersect(ray, node.payload());
                    if min_distance <= t && t < max_distance {
                        return true;
                    }
                } else {
                    let offset = node.payload();
                    let sign = signs[node.split_axis()];
                    stack.push(node_index + offset + sign);
                    node_index += offset + (sign ^ 1);
                    continue;
                }
            }

            match stack.pop() {
                Some(next) => node_index = next,
                None => break,
            }
        }

        false
    }

    /// `true` if the segment from `start` to `end` hits a triangle, with no tolerance at either end (upstream
    /// `isOccluded(start, end, mesh)`).
    pub fn is_occluded(&self, start: Vector3, end: Vector3, mesh: &Mesh) -> bool {
        let ray = Ray::new(start, (end - start).normalize_or_zero());
        let distance = (end - start).length();
        self.any_hit(&ray, mesh, 0.0, distance)
    }

    /// `true` if any triangle intersects `aabb` (upstream `intersect(box, mesh)`).
    pub fn intersects_aabb(&self, aabb: &Aabb, mesh: &Mesh) -> bool {
        let mut stack = TraversalStack::<usize>::new();
        let mut node_index = 0;

        loop {
            let node = &self.nodes[node_index];

            if aabbs_intersect(aabb, &node.bounds) {
                if node.is_leaf() {
                    if aabb_intersects_triangle(aabb, mesh, node.payload()) {
                        return true;
                    }
                } else {
                    let axis = node.split_axis();
                    let mut near = node.payload();
                    let mut far = near + 1;
                    if aabb.min[axis] > node.bounds.min[axis] {
                        std::mem::swap(&mut near, &mut far);
                    }

                    stack.push(node_index + far);
                    node_index += near;
                    continue;
                }
            }

            match stack.pop() {
                Some(next) => node_index = next,
                None => break,
            }
        }

        false
    }
}

/// Upstream `bestSplit`: SAH, or the object median if no SAH split has a finite cost.
fn best_split(
    leaf_boxes: &[GrowableBox],
    leaf_indices: &mut [u32],
    centroids: &mut [Vec<Centroid>; 3],
    surface_areas: &mut [f32],
    bounds: &GrowableBox,
    start: usize,
    end: usize,
) -> Split {
    sah_split(
        leaf_boxes,
        leaf_indices,
        centroids,
        surface_areas,
        bounds,
        start,
        end,
    )
    .unwrap_or_else(|| median_split(leaf_indices, centroids, bounds, start, end))
}

/// Upstream `medianSplit`: halves the leaves sorted along the longest axis (sorted by the SAH attempt).
fn median_split(
    leaf_indices: &mut [u32],
    centroids: &[Vec<Centroid>; 3],
    bounds: &GrowableBox,
    start: usize,
    end: usize,
) -> Split {
    let axis = bounds.to_aabb().extents().index_of_max_component();
    for i in start..=end {
        leaf_indices[i] = centroids[axis][i].leaf;
    }
    Split {
        index: (end - start).div_ceil(2),
        axis,
    }
}

/// Upstream `sahSplit`. Sorts the leaves' centroids along every axis, and returns `None` if no split cost is
/// below `f32::MAX` (for example when the parent's surface area is 0).
fn sah_split(
    leaf_boxes: &[GrowableBox],
    leaf_indices: &mut [u32],
    centroids: &mut [Vec<Centroid>; 3],
    surface_areas: &mut [f32],
    bounds: &GrowableBox,
    start: usize,
    end: usize,
) -> Option<Split> {
    let parent_surface_area = bounds.surface_area();
    let mut best_cost = f32::MAX;
    let mut split = None;
    let half = (end - start).div_ceil(2) as i64;

    for (axis, centroids) in centroids.iter_mut().enumerate() {
        let mut best_balance_cost = i64::from(i32::MAX);

        let centroids = &mut centroids[start..=end];
        msvc_sort::sort(centroids, |a, b| a.coordinate < b.coordinate);

        // `centroids[k]` is leaf `start + k`.
        let mut left_box = GrowableBox::EMPTY;
        for k in 0..end - start {
            left_box.grow_to_contain(&leaf_boxes[centroids[k].leaf as usize]);
            surface_areas[start + k] = left_box.surface_area();
        }

        // Splitting before sorted leaf `k` puts `k` leaves on the left.
        let mut right_box = GrowableBox::EMPTY;
        for k in (1..=end - start).rev() {
            let num_left = k;
            let num_right = end - start + 1 - k;
            right_box.grow_to_contain(&leaf_boxes[centroids[k].leaf as usize]);
            let cost = (surface_areas[start + k - 1] * num_left as f32
                + right_box.surface_area() * num_right as f32)
                / parent_surface_area;

            if cost < best_cost {
                best_cost = cost;
                split = Some(Split {
                    index: num_left,
                    axis,
                });
            } else if cost == best_cost {
                let balance_cost = (num_left as i64 - half).abs();
                if balance_cost < best_balance_cost {
                    best_balance_cost = balance_cost;
                    split = Some(Split {
                        index: num_left,
                        axis,
                    });
                }
            }
        }
    }

    if let Some(split) = split {
        for i in start..=end {
            leaf_indices[i] = centroids[split.axis][i].leaf;
        }
    }

    split
}

/// Upstream `boxIntersectsBox`: `true` if the boxes overlap or touch.
pub(crate) fn aabbs_intersect(a: &Aabb, b: &Aabb) -> bool {
    let gap = |axis: usize| {
        cpp_max(0.0, b.min[axis] - a.max[axis]) + cpp_max(0.0, a.min[axis] - b.max[axis])
    };
    gap(0) == 0.0 && gap(1) == 0.0 && gap(2) == 0.0
}

/// Upstream `boxIntersectsTriangle`: plane test, then separating-axis tests of the triangle's edge normals
/// projected onto the xy, yz and zx planes.
fn aabb_intersects_triangle(aabb: &Aabb, mesh: &Mesh, triangle: usize) -> bool {
    let [v0, v1, v2] = mesh.triangle_vertices(triangle);
    let normal = mesh.normal(triangle);
    let extents = aabb.extents();

    let mut critical_point_offset = Vector3::ZERO;
    for axis in 0..3 {
        if normal[axis] > 0.0 {
            critical_point_offset[axis] = extents[axis];
        }
    }

    let np = normal.dot(aabb.min);
    let d1 = normal.dot(critical_point_offset - v0);
    let d2 = normal.dot((extents - critical_point_offset) - v0);

    if (np + d1) * (np + d2) > 0.0 {
        return false;
    }

    let e0 = v1 - v0;
    let e1 = v2 - v1;
    let e2 = v0 - v2;

    // Projections onto the planes (a, b) = (x, y), (y, z), (z, x); the sign of the normal's remaining component
    // orients the edge normals.
    for (a, b, c) in [(0, 1, 2), (1, 2, 0), (2, 0, 1)] {
        let flip = normal[c] < 0.0;
        let edge_normal = |e: Vector3| {
            let mut n = Vector2::new(-e[b], e[a]);
            if flip {
                n *= -1.0;
            }
            n
        };
        let n0 = edge_normal(e0);
        let n1 = edge_normal(e1);
        let n2 = edge_normal(e2);

        let offset = |n: Vector2, v: Vector3| {
            -n.dot(Vector2::new(v[a], v[b]))
                + cpp_max(0.0, extents[a] * n.x)
                + cpp_max(0.0, extents[b] * n.y)
        };
        let d0 = offset(n0, v0);
        let d1 = offset(n1, v1);
        let d2 = offset(n2, v2);

        let corner = Vector2::new(aabb.min[a], aabb.min[b]);
        if n0.dot(corner) + d0 < 0.0 || n1.dot(corner) + d1 < 0.0 || n2.dot(corner) + d2 < 0.0 {
            return false;
        }
    }

    true
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn growable_box_matches_upstream_tests() {
        let mut a = GrowableBox::EMPTY;
        a.grow_to_contain_point(Vector3::new(1.0, 2.0, 3.0));
        assert_eq!(a.min, [1.0, 2.0, 3.0]);
        assert_eq!(a.max, [1.0, 2.0, 3.0]);

        let mut b = GrowableBox::EMPTY;
        b.grow_to_contain_point(Vector3::new(10.0, 12.0, -32.0));
        a.grow_to_contain(&b);
        assert_eq!(a.min, [1.0, 2.0, -32.0]);
        assert_eq!(a.max, [10.0, 12.0, 3.0]);
        assert_eq!(a.surface_area(), 1510.0);
    }

    #[test]
    fn traversal_stack_spills() {
        let mut stack = TraversalStack::<usize>::new();
        for i in 0..300 {
            stack.push(i);
        }
        for i in (0..300).rev() {
            assert_eq!(stack.pop(), Some(i));
        }
        assert_eq!(stack.pop(), None);
    }
}
