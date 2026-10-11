//! Nearest-neighbour search over 3-D points (upstream `hrtf/kdtree.c`, from John Tsiombikas's `kdtree` library with
//! Tamas Nepusz's single nearest neighbour search; BSD 3-Clause).
//!
//! The tree is built by plain insertion, never rebalanced, so its shape depends on the insertion order; the search
//! visits nodes in upstream's order, which decides between equidistant points. Nodes live in an arena and the search
//! uses an explicit stack, so degenerate (deep) trees cannot overflow the call stack.

/// `struct kdnode`.
#[derive(Debug, Clone)]
struct Node {
    pos: [f32; 3],
    dir: usize,
    data: usize,
    left: Option<u32>,
    right: Option<u32>,
}

/// `struct kdhyperrect`.
#[derive(Debug, Clone, Copy)]
struct Rect {
    min: [f32; 3],
    max: [f32; 3],
}

impl Rect {
    /// `hyperrect_extend`.
    fn extend(&mut self, pos: &[f32; 3]) {
        for ((&p, min), max) in pos.iter().zip(&mut self.min).zip(&mut self.max) {
            if p < *min {
                *min = p;
            }
            if p > *max {
                *max = p;
            }
        }
    }

    /// `hyperrect_dist_sq`.
    fn dist_sq(&self, pos: &[f32; 3]) -> f32 {
        let mut result = 0.0;
        for ((&p, &min), &max) in pos.iter().zip(&self.min).zip(&self.max) {
            if p < min {
                result += sq(min - p);
            } else if p > max {
                result += sq(max - p);
            }
        }
        result
    }

    /// The coordinate of the subtree on the near (`near == true`) or far side of a split, given which way the query
    /// went (`left`).
    fn side(&mut self, left: bool, near: bool, dir: usize) -> &mut f32 {
        if left == near {
            &mut self.max[dir]
        } else {
            &mut self.min[dir]
        }
    }
}

fn sq(x: f32) -> f32 {
    x * x
}

fn dist_sq(a: &[f32; 3], b: &[f32; 3]) -> f32 {
    let mut result = 0.0;
    for (&a, &b) in a.iter().zip(b) {
        result += sq(a - b);
    }
    result
}

/// One level of upstream's recursive `kd_nearest_i`.
pub(crate) struct Frame {
    node: u32,
    /// 0: about to visit the nearer subtree; 1: back from it; 2: back from the farther subtree.
    stage: u8,
    /// The rect coordinate the current slice replaced.
    saved: f32,
}

/// Scratch space for [`KdTree::nearest`].
pub(crate) type Stack = Vec<Frame>;

/// `struct kdtree`.
#[derive(Debug, Clone, Default)]
pub(crate) struct KdTree {
    nodes: Vec<Node>,
    rect: Option<Rect>,
    depth: usize,
}

impl KdTree {
    /// `kd_insert`.
    pub(crate) fn insert(&mut self, pos: [f32; 3], data: usize) {
        let new = self.nodes.len() as u32;
        let mut dir = 0;
        let mut depth = 1;
        let mut link = None;
        let mut current = (!self.nodes.is_empty()).then_some(0);
        while let Some(index) = current {
            let node = &self.nodes[index as usize];
            dir = (node.dir + 1) % 3;
            let left = pos[node.dir] < node.pos[node.dir];
            current = if left { node.left } else { node.right };
            link = Some((index, left));
            depth += 1;
        }
        if let Some((parent, left)) = link {
            let parent = &mut self.nodes[parent as usize];
            if left {
                parent.left = Some(new);
            } else {
                parent.right = Some(new);
            }
        }
        self.nodes.push(Node {
            pos,
            dir,
            data,
            left: None,
            right: None,
        });
        self.depth = self.depth.max(depth);
        match &mut self.rect {
            None => self.rect = Some(Rect { min: pos, max: pos }),
            Some(rect) => rect.extend(&pos),
        }
    }

    /// Creates scratch space for [`KdTree::nearest`] large enough for this tree.
    pub(crate) fn stack(&self) -> Stack {
        Vec::with_capacity(self.depth)
    }

    /// `kd_nearest`: the data of the nearest node, `None` if the tree is empty.
    pub(crate) fn nearest(&self, pos: &[f32; 3], stack: &mut Stack) -> Option<usize> {
        let mut rect = self.rect?;
        let mut result = 0;
        let mut result_dist_sq = dist_sq(&self.nodes[0].pos, pos);

        stack.clear();
        stack.push(Frame {
            node: 0,
            stage: 0,
            saved: 0.0,
        });
        while let Some(frame) = stack.last_mut() {
            let node = &self.nodes[frame.node as usize];
            let dir = node.dir;
            // `dummy <= 0` is false for NaN, which goes right.
            let left = pos[dir] - node.pos[dir] <= 0.0;
            let (nearer, farther) = if left {
                (node.left, node.right)
            } else {
                (node.right, node.left)
            };
            match frame.stage {
                0 => {
                    frame.stage = 1;
                    if let Some(child) = nearer {
                        // Slice the rect to the nearer subtree.
                        let coord = rect.side(left, true, dir);
                        frame.saved = *coord;
                        *coord = node.pos[dir];
                        stack.push(Frame {
                            node: child,
                            stage: 0,
                            saved: 0.0,
                        });
                    }
                }
                1 => {
                    if nearer.is_some() {
                        *rect.side(left, true, dir) = frame.saved;
                    }
                    let d = dist_sq(&node.pos, pos);
                    if d < result_dist_sq {
                        result = frame.node;
                        result_dist_sq = d;
                    }
                    let Some(child) = farther else {
                        stack.pop();
                        continue;
                    };
                    frame.stage = 2;
                    let coord = rect.side(left, false, dir);
                    frame.saved = *coord;
                    *coord = node.pos[dir];
                    if rect.dist_sq(pos) < result_dist_sq {
                        stack.push(Frame {
                            node: child,
                            stage: 0,
                            saved: 0.0,
                        });
                    }
                }
                _ => {
                    *rect.side(left, false, dir) = frame.saved;
                    stack.pop();
                }
            }
        }
        Some(self.nodes[result as usize].data)
    }
}
