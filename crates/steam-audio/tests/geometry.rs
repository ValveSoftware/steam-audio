//! Parity tests for stage 2.1 against golden data from `tools/golden/src/geometry.cpp`.
//!
//! Each test replays the generator's `Lcg` draws in exactly the same order, recomputes the outputs with the
//! Rust implementation, and compares them with the reference.
//!
//! On Windows every value currently matches the reference bit for bit (Rust and MSVC share the CRT's libm).
//! The `F32` tolerance absorbs last-bit differences in other platforms' `sin`/`atan2`/`asin`.

use parity::{Lcg, Tolerance, golden};
use steam_audio::geometry::{Aabb, Ray, Sphere};
use steam_audio::math::{
    CoordinateSpace3, CylindricalVector3, InterauralSphericalVector3, Matrix, Matrix2, Matrix3,
    Matrix4, Quaternion, SphericalVector3, Vector3, Vector4,
};

const NUM_CASES: usize = 256;

fn random_vector3(lcg: &mut Lcg, scale: f32) -> Vector3 {
    let x = lcg.next_signed() * scale;
    let y = lcg.next_signed() * scale;
    let z = lcg.next_signed() * scale;
    Vector3::new(x, y, z)
}

fn random_matrix<const R: usize, const C: usize>(lcg: &mut Lcg, scale: f32) -> Matrix<R, C> {
    let mut m = Matrix::<R, C>::ZERO;
    for i in 0..R {
        for j in 0..C {
            m[(i, j)] = lcg.next_signed() * scale;
        }
    }
    m
}

/// Accumulates rows like the generator's `Table`.
#[derive(Default)]
struct Rows<T>(Vec<T>);

impl<T: Copy> Rows<T> {
    fn push(&mut self, row: impl IntoIterator<Item = T>) {
        self.0.extend(row);
    }
}

#[track_caller]
fn check<T: Into<f64> + Copy + parity::Element>(name: &str, actual: &Rows<T>, tol: Tolerance) {
    let expected = golden::<T>(name);
    assert_eq!(expected.shape[0], NUM_CASES, "{name}");
    if let Err(m) = parity::compare(&actual.0, &expected.data, tol) {
        panic!("{name}: {m}");
    }
}

#[test]
fn vector() {
    let mut lcg = Lcg::new(1);
    let mut dot = Rows::default();
    let mut cross = Rows::default();
    let mut length = Rows::default();
    let mut unit = Rows::default();
    let mut div_scalar = Rows::default();
    let mut angle_between = Rows::default();
    let mut reflect = Rows::default();
    let mut min = Rows::default();
    let mut max = Rows::default();
    let mut components = Rows::default();
    let mut indices = Rows::<i32>::default();
    let mut reciprocal = Rows::default();
    let mut sqrt = Rows::default();

    for i in 0..NUM_CASES {
        let mut a = random_vector3(&mut lcg, 10.0);
        let b = random_vector3(&mut lcg, 10.0);
        let s = lcg.next_signed() * 10.0;
        if i % 16 == 0 {
            a *= 1e-7;
        }

        dot.push([a.dot(b)]);
        cross.push(a.cross(b).to_array());
        length.push([a.length()]);
        unit.push(a.normalize_or_zero().to_array());
        div_scalar.push((a / s).to_array());
        angle_between.push([Vector3::angle_between(a.normalize_or_zero(), b)]);
        reflect.push(Vector3::reflect(a, b.normalize_or_zero()).to_array());
        min.push(a.min(b).to_array());
        max.push(a.max(b).to_array());
        components.push([
            a.min_component(),
            a.max_component(),
            a.min_abs_component(),
            a.max_abs_component(),
        ]);
        indices.push(
            [
                a.index_of_min_component(),
                a.index_of_max_component(),
                a.index_of_min_abs_component(),
                a.index_of_max_abs_component(),
            ]
            .map(|i| i as i32),
        );
        reciprocal.push(b.recip().to_array());
        sqrt.push(
            Vector3::new(b.x.abs(), b.y.abs(), b.z.abs())
                .sqrt()
                .to_array(),
        );
    }

    check("vector/dot", &dot, Tolerance::F32);
    check("vector/cross", &cross, Tolerance::F32);
    check("vector/length", &length, Tolerance::F32);
    check("vector/unit", &unit, Tolerance::F32);
    check("vector/div_scalar", &div_scalar, Tolerance::F32);
    check("vector/angle_between", &angle_between, Tolerance::F32);
    check("vector/reflect", &reflect, Tolerance::F32);
    check("vector/min", &min, Tolerance::EXACT);
    check("vector/max", &max, Tolerance::EXACT);
    check("vector/components", &components, Tolerance::EXACT);
    check("vector/component_indices", &indices, Tolerance::EXACT);
    check("vector/reciprocal", &reciprocal, Tolerance::F32);
    check("vector/sqrt", &sqrt, Tolerance::F32);
}

#[test]
fn matrix() {
    let mut lcg = Lcg::new(2);
    let mut det2 = Rows::default();
    let mut det3 = Rows::default();
    let mut det4 = Rows::default();
    let mut inverse = Rows::default();
    let mut product = Rows::default();
    let mut mul_vector = Rows::default();
    let mut transposed = Rows::default();

    for _ in 0..NUM_CASES {
        let a: Matrix4 = random_matrix(&mut lcg, 4.0);
        let b: Matrix4 = random_matrix(&mut lcg, 4.0);
        let x = lcg.next_signed() * 4.0;
        let y = lcg.next_signed() * 4.0;
        let z = lcg.next_signed() * 4.0;
        let w = lcg.next_signed() * 4.0;
        let v = Vector4::new(x, y, z, w);
        let m3: Matrix3 = random_matrix(&mut lcg, 4.0);
        let m2: Matrix2 = random_matrix(&mut lcg, 4.0);

        det2.push([m2.determinant()]);
        det3.push([m3.determinant()]);
        det4.push([a.determinant()]);
        inverse.push(a.inverse_or_zero().column_major());
        product.push((a * b).column_major());
        mul_vector.push((a * v).to_array());
        transposed.push(a.transpose().column_major());
    }

    check("matrix/det2", &det2, Tolerance::F32);
    check("matrix/det3", &det3, Tolerance::F32);
    check("matrix/det4", &det4, Tolerance::F32);
    check("matrix/inverse4", &inverse, Tolerance::F32);
    check("matrix/product4", &product, Tolerance::F32);
    check("matrix/mul_vector4", &mul_vector, Tolerance::F32);
    check("matrix/transposed4", &transposed, Tolerance::EXACT);
}

#[test]
fn quaternion() {
    let mut lcg = Lcg::new(3);
    let mut length = Rows::default();
    let mut normalized = Rows::default();
    let mut rotation = Rows::default();
    let mut product = Rows::default();

    let random_quaternion = |lcg: &mut Lcg| {
        let x = lcg.next_signed() * 2.0;
        let y = lcg.next_signed() * 2.0;
        let z = lcg.next_signed() * 2.0;
        let w = lcg.next_signed() * 2.0;
        Quaternion::new(x, y, z, w)
    };

    for _ in 0..NUM_CASES {
        let mut q = random_quaternion(&mut lcg);
        let r = random_quaternion(&mut lcg);

        length.push([q.length()]);
        let p = q * r;
        product.push([p.x, p.y, p.z, p.w]);

        q.normalize();
        normalized.push([q.x, q.y, q.z, q.w]);
        rotation.push(q.to_rotation_matrix().column_major());
    }

    check("quaternion/length", &length, Tolerance::F32);
    check("quaternion/normalized", &normalized, Tolerance::F32);
    check("quaternion/rotation", &rotation, Tolerance::F32);
    check("quaternion/product", &product, Tolerance::F32);
}

#[test]
fn coordinate_space() {
    let mut lcg = Lcg::new(4);
    let mut basis = Rows::default();
    let mut rotation = Rows::default();
    let mut world_to_local = Rows::default();
    let mut local_to_world = Rows::default();

    for _ in 0..NUM_CASES {
        let ahead = random_vector3(&mut lcg, 1.0).normalize_or_zero();
        let origin = random_vector3(&mut lcg, 10.0);
        let d = random_vector3(&mut lcg, 1.0);

        let space = CoordinateSpace3::from_ahead(ahead, origin);
        basis.push(space.right.to_array());
        basis.push(space.up.to_array());
        basis.push(space.ahead.to_array());
        rotation.push(space.to_rotation_matrix().column_major());
        world_to_local.push(space.direction_world_to_local(d).to_array());
        local_to_world.push(space.direction_local_to_world(d).to_array());
    }

    check("coordinate_space/basis", &basis, Tolerance::F32);
    check("coordinate_space/rotation", &rotation, Tolerance::F32);
    check(
        "coordinate_space/world_to_local",
        &world_to_local,
        Tolerance::F32,
    );
    check(
        "coordinate_space/local_to_world",
        &local_to_world,
        Tolerance::F32,
    );
}

#[test]
fn polar_vector() {
    let mut lcg = Lcg::new(5);
    let mut spherical = Rows::default();
    let mut spherical_to_cartesian = Rows::default();
    let mut interaural = Rows::default();
    let mut interaural_to_cartesian = Rows::default();
    let mut cylindrical = Rows::default();
    let mut cylindrical_to_cartesian = Rows::default();

    for i in 0..NUM_CASES {
        let mut c = random_vector3(&mut lcg, 5.0);
        match i % 32 {
            0 => c = Vector3::new(0.0, 3.0, 0.0),
            1 => c = Vector3::new(0.0, -2.0, 0.0),
            2 => c = Vector3::new(4.0, 0.0, 0.0),
            3 => c = Vector3::new(0.0, 1.0, 1e-7),
            _ => {}
        }

        let s = SphericalVector3::from(c);
        spherical.push([s.radius, s.elevation, s.azimuth]);
        spherical_to_cartesian.push(s.to_cartesian().to_array());

        let ia = InterauralSphericalVector3::from(c);
        interaural.push([ia.radius, ia.azimuth, ia.elevation]);
        interaural_to_cartesian.push(ia.to_cartesian().to_array());

        let cy = CylindricalVector3::from(c);
        cylindrical.push([cy.radius, cy.height, cy.azimuth]);
        cylindrical_to_cartesian.push(cy.to_cartesian().to_array());
    }

    check("polar_vector/spherical", &spherical, Tolerance::F32);
    check(
        "polar_vector/spherical_to_cartesian",
        &spherical_to_cartesian,
        Tolerance::F32,
    );
    check("polar_vector/interaural", &interaural, Tolerance::F32);
    check(
        "polar_vector/interaural_to_cartesian",
        &interaural_to_cartesian,
        Tolerance::F32,
    );
    check("polar_vector/cylindrical", &cylindrical, Tolerance::F32);
    check(
        "polar_vector/cylindrical_to_cartesian",
        &cylindrical_to_cartesian,
        Tolerance::F32,
    );
}

#[test]
fn shapes() {
    let mut lcg = Lcg::new(6);
    let mut bounding_sphere = Rows::default();
    let mut sphere_contains = Rows::<i32>::default();
    let mut box_contains = Rows::<i32>::default();
    let mut box_center = Rows::default();
    let mut box_extents = Rows::default();
    let mut box_surface_area = Rows::default();
    let mut ray_triangle = Rows::default();
    let mut ray_box = Rows::default();
    let mut ray_sphere = Rows::default();

    for i in 0..NUM_CASES {
        let center = random_vector3(&mut lcg, 5.0);
        let a = Sphere::new(center, lcg.next_signed().abs() * 3.0);
        let center = random_vector3(&mut lcg, 5.0);
        let mut b = Sphere::new(center, lcg.next_signed().abs() * 3.0);
        if i % 8 == 0 {
            b = Sphere::new(a.center + Vector3::new(0.1, 0.0, 0.0), a.radius * 0.25);
        }
        let point = random_vector3(&mut lcg, 5.0);

        let bounding = Sphere::bounding(&a, &b);
        bounding_sphere.push([
            bounding.center.x,
            bounding.center.y,
            bounding.center.z,
            bounding.radius,
        ]);
        sphere_contains.push([i32::from(a.contains(point))]);

        let corner0 = random_vector3(&mut lcg, 5.0);
        let corner1 = random_vector3(&mut lcg, 5.0);
        let aabb = Aabb::new(corner0.min(corner1), corner0.max(corner1));
        box_contains.push([i32::from(aabb.contains(point))]);
        box_center.push(aabb.center().to_array());
        box_extents.push(aabb.extents().to_array());
        box_surface_area.push([aabb.surface_area()]);

        let origin = random_vector3(&mut lcg, 5.0);
        let direction = random_vector3(&mut lcg, 1.0).normalize_or_zero();
        let ray = Ray::new(origin, direction);

        let v0 = random_vector3(&mut lcg, 3.0);
        let v1 = random_vector3(&mut lcg, 3.0);
        let v2 = random_vector3(&mut lcg, 3.0);
        ray_triangle.push([ray.intersect_triangle(v0, v1, v2)]);

        let signs = direction.to_array().map(|d| usize::from(d >= 0.0));
        let (mut t_min, mut t_max) = (0.0, f32::INFINITY);
        let hit = ray.intersect_aabb(&aabb, direction.recip(), signs, &mut t_min, &mut t_max);
        ray_box.push([if hit { 1.0 } else { 0.0 }, t_min, t_max]);

        ray_sphere.push([ray.intersect_sphere(&a)]);
    }

    check("shapes/bounding_sphere", &bounding_sphere, Tolerance::F32);
    check("shapes/sphere_contains", &sphere_contains, Tolerance::EXACT);
    check("shapes/box_contains", &box_contains, Tolerance::EXACT);
    check("shapes/box_center", &box_center, Tolerance::F32);
    check("shapes/box_extents", &box_extents, Tolerance::F32);
    check("shapes/box_surface_area", &box_surface_area, Tolerance::F32);
    check("shapes/ray_triangle", &ray_triangle, Tolerance::F32);
    check("shapes/ray_box", &ray_box, Tolerance::F32);
    check("shapes/ray_sphere", &ray_sphere, Tolerance::F32);
}

#[test]
fn golden_shapes_have_expected_widths() {
    // Guards against the generator and the test disagreeing on layout.
    assert_eq!(golden::<f32>("matrix/inverse4").shape, [NUM_CASES, 16]);
    assert_eq!(
        golden::<i32>("vector/component_indices").shape,
        [NUM_CASES, 4]
    );
}
