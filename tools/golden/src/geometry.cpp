// Golden data for stage 2.1: vector.h, matrix.h, quaternion.h, coordinate_space.h, polar_vector.h, sphere.h,
// box.h, ray.{h,cpp}.
//
// Every case draws its inputs from an Lcg in a fixed order, one value per statement (C++ leaves the evaluation order
// of function arguments unspecified). The Rust tests in crates/steam-audio/tests/geometry.rs replay the same order.

#include <box.h>
#include <coordinate_space.h>
#include <matrix.h>
#include <mesh.h>
#include <polar_vector.h>
#include <quaternion.h>
#include <ray.h>
#include <sphere.h>
#include <vector.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using namespace ipl;

constexpr int kNumCases = 256;

// Collects rows of equal width into an [N, width] array.
template <typename T>
class Table
{
public:
    explicit Table(size_t width) : mWidth(width) {}

    void push(std::initializer_list<T> row) { mData.insert(mData.end(), row.begin(), row.end()); }
    void push(const Vector3f& v) { push({v.x(), v.y(), v.z()}); }
    void push(const Vector4f& v) { push({v.x(), v.y(), v.z(), v.w()}); }

    template <int R, int C>
    void push(const Matrix<float, R, C>& m)
    {
        // Column-major, exactly as stored.
        mData.insert(mData.end(), m.elements, m.elements + R * C);
    }

    void write(const std::filesystem::path& path) const
    {
        if (mWidth == 1)
            writeNpy(path, {mData.size()}, mData.data());
        else
            writeNpy(path, {mData.size() / mWidth, mWidth}, mData.data());
    }

private:
    size_t mWidth;
    std::vector<T> mData;
};

Vector3f randomVector3(Lcg& lcg, float scale)
{
    Vector3f v;
    v.x() = lcg.nextSigned() * scale;
    v.y() = lcg.nextSigned() * scale;
    v.z() = lcg.nextSigned() * scale;
    return v;
}

template <int R, int C>
void fillMatrix(Lcg& lcg, Matrix<float, R, C>& m, float scale)
{
    // Row by row, in (row, col) order.
    for (int i = 0; i < R; ++i)
        for (int j = 0; j < C; ++j)
            m(i, j) = lcg.nextSigned() * scale;
}

void generateVector(const std::filesystem::path& out)
{
    Lcg lcg(1);
    Table<float> dot(1), cross(3), length(1), unit(3), divScalar(3), angleBetween(1), reflect(3);
    Table<float> min(3), max(3), components(4), reciprocal(3), sqrt(3);
    Table<int32_t> indices(4);

    for (int i = 0; i < kNumCases; ++i)
    {
        auto a = randomVector3(lcg, 10.0f);
        auto b = randomVector3(lcg, 10.0f);
        auto s = lcg.nextSigned() * 10.0f;
        if (i % 16 == 0)
            a *= 1e-7f; // Below Vector3::kNearlyZero: unitVector() returns zero.

        dot.push({Vector3f::dot(a, b)});
        cross.push(Vector3f::cross(a, b));
        length.push({a.length()});
        unit.push(Vector3f::unitVector(a));
        divScalar.push(Vector3f(a / s));
        angleBetween.push({Vector3f::angleBetween(Vector3f::unitVector(a), b)});
        reflect.push(Vector3f::reflect(a, Vector3f::unitVector(b)));
        min.push(Vector3f(Vector3f::min(a, b)));
        max.push(Vector3f(Vector3f::max(a, b)));
        components.push({a.minComponent(), a.maxComponent(), a.minAbsComponent(), a.maxAbsComponent()});
        indices.push({a.indexOfMinComponent(), a.indexOfMaxComponent(), a.indexOfMinAbsComponent(),
                      a.indexOfMaxAbsComponent()});
        reciprocal.push(Vector3f(Vector3f::reciprocal(b)));
        sqrt.push(Vector3f(Vector3f::sqrt(Vector3f(std::abs(b.x()), std::abs(b.y()), std::abs(b.z())))));
    }

    dot.write(out / "dot.npy");
    cross.write(out / "cross.npy");
    length.write(out / "length.npy");
    unit.write(out / "unit.npy");
    divScalar.write(out / "div_scalar.npy");
    angleBetween.write(out / "angle_between.npy");
    reflect.write(out / "reflect.npy");
    min.write(out / "min.npy");
    max.write(out / "max.npy");
    components.write(out / "components.npy");
    indices.write(out / "component_indices.npy");
    reciprocal.write(out / "reciprocal.npy");
    sqrt.write(out / "sqrt.npy");
}

void generateMatrix(const std::filesystem::path& out)
{
    Lcg lcg(2);
    Table<float> det2(1), det3(1), det4(1), inverse(16), product(16), mulVector(4), transposed(16);

    for (int i = 0; i < kNumCases; ++i)
    {
        Matrix4x4f a, b;
        fillMatrix(lcg, a, 4.0f);
        fillMatrix(lcg, b, 4.0f);
        Vector4f v;
        v.x() = lcg.nextSigned() * 4.0f;
        v.y() = lcg.nextSigned() * 4.0f;
        v.z() = lcg.nextSigned() * 4.0f;
        v.w() = lcg.nextSigned() * 4.0f;
        Matrix3x3f m3;
        fillMatrix(lcg, m3, 4.0f);
        Matrix2x2f m2;
        fillMatrix(lcg, m2, 4.0f);

        det2.push({determinant(m2)});
        det3.push({determinant(m3)});
        det4.push({determinant(a)});

        Matrix4x4f aInv;
        ipl::inverse(a, aInv);
        inverse.push(aInv);

        product.push(Matrix<float, 4, 4>(a * b));
        mulVector.push(Vector4f(a * v));
        transposed.push(a.transposedCopy());
    }

    det2.write(out / "det2.npy");
    det3.write(out / "det3.npy");
    det4.write(out / "det4.npy");
    inverse.write(out / "inverse4.npy");
    product.write(out / "product4.npy");
    mulVector.write(out / "mul_vector4.npy");
    transposed.write(out / "transposed4.npy");
}

void generateQuaternion(const std::filesystem::path& out)
{
    Lcg lcg(3);
    Table<float> length(1), normalized(4), rotation(9), product(4);

    for (int i = 0; i < kNumCases; ++i)
    {
        Quaternionf q;
        q.x = lcg.nextSigned() * 2.0f;
        q.y = lcg.nextSigned() * 2.0f;
        q.z = lcg.nextSigned() * 2.0f;
        q.w = lcg.nextSigned() * 2.0f;
        Quaternionf r;
        r.x = lcg.nextSigned() * 2.0f;
        r.y = lcg.nextSigned() * 2.0f;
        r.z = lcg.nextSigned() * 2.0f;
        r.w = lcg.nextSigned() * 2.0f;

        length.push({q.length()});
        auto p = q * r;
        product.push({p.x, p.y, p.z, p.w});

        q.normalize();
        normalized.push({q.x, q.y, q.z, q.w});
        rotation.push(Matrix<float, 3, 3>(q.toRotationMatrix()));
    }

    length.write(out / "length.npy");
    normalized.write(out / "normalized.npy");
    rotation.write(out / "rotation.npy");
    product.write(out / "product.npy");
}

void generateCoordinateSpace(const std::filesystem::path& out)
{
    Lcg lcg(4);
    Table<float> basis(9), rotation(9), worldToLocal(3), localToWorld(3);

    for (int i = 0; i < kNumCases; ++i)
    {
        auto ahead = Vector3f::unitVector(randomVector3(lcg, 1.0f));
        auto origin = randomVector3(lcg, 10.0f);
        auto d = randomVector3(lcg, 1.0f);

        CoordinateSpace3f space(ahead, origin);
        basis.push({space.right.x(), space.right.y(), space.right.z(), space.up.x(), space.up.y(), space.up.z(),
                    space.ahead.x(), space.ahead.y(), space.ahead.z()});
        rotation.push(Matrix<float, 3, 3>(space.toRotationMatrix()));
        worldToLocal.push(space.transformDirectionFromWorldToLocal(d));
        localToWorld.push(space.transformDirectionFromLocalToWorld(d));
    }

    basis.write(out / "basis.npy");
    rotation.write(out / "rotation.npy");
    worldToLocal.write(out / "world_to_local.npy");
    localToWorld.write(out / "local_to_world.npy");
}

void generatePolar(const std::filesystem::path& out)
{
    Lcg lcg(5);
    Table<float> spherical(3), sphericalToCartesian(3), interaural(3), interauralToCartesian(3);
    Table<float> cylindrical(3), cylindricalToCartesian(3);

    for (int i = 0; i < kNumCases; ++i)
    {
        auto c = randomVector3(lcg, 5.0f);
        // Poles exercise the special cases (azimuth or elevation forced to 0).
        if (i % 32 == 0)
            c = Vector3f(0.0f, 3.0f, 0.0f);
        else if (i % 32 == 1)
            c = Vector3f(0.0f, -2.0f, 0.0f);
        else if (i % 32 == 2)
            c = Vector3f(4.0f, 0.0f, 0.0f);
        else if (i % 32 == 3)
            c = Vector3f(0.0f, 1.0f, 1e-7f);

        SphericalVector3f s(c);
        spherical.push({s.radius, s.elevation, s.azimuth});
        sphericalToCartesian.push(s.toCartesian());

        InterauralSphericalVector3f ia(c);
        interaural.push({ia.radius, ia.azimuth, ia.elevation});
        interauralToCartesian.push(ia.toCartesian());

        CylindricalVector3f cy(c);
        cylindrical.push({cy.radius, cy.height, cy.azimuth});
        cylindricalToCartesian.push(cy.toCartesian());
    }

    spherical.write(out / "spherical.npy");
    sphericalToCartesian.write(out / "spherical_to_cartesian.npy");
    interaural.write(out / "interaural.npy");
    interauralToCartesian.write(out / "interaural_to_cartesian.npy");
    cylindrical.write(out / "cylindrical.npy");
    cylindricalToCartesian.write(out / "cylindrical_to_cartesian.npy");
}

void generateShapes(const std::filesystem::path& out)
{
    Lcg lcg(6);
    Table<float> boundingSphere(4), boxCenter(3), boxExtents(3), boxSurfaceArea(1);
    Table<float> rayTriangle(1), rayBox(3), raySphere(1);
    Table<int32_t> sphereContains(1), boxContains(1);

    for (int i = 0; i < kNumCases; ++i)
    {
        Sphere a;
        a.center = randomVector3(lcg, 5.0f);
        a.radius = std::abs(lcg.nextSigned()) * 3.0f;
        Sphere b;
        b.center = randomVector3(lcg, 5.0f);
        b.radius = std::abs(lcg.nextSigned()) * 3.0f;
        if (i % 8 == 0)
        {
            // Nested spheres.
            b.center = a.center + Vector3f(0.1f, 0.0f, 0.0f);
            b.radius = a.radius * 0.25f;
        }
        auto point = randomVector3(lcg, 5.0f);

        auto bounding = computeBoundingSphere(a, b);
        boundingSphere.push({bounding.center.x(), bounding.center.y(), bounding.center.z(), bounding.radius});
        sphereContains.push({a.contains(point) ? 1 : 0});

        auto corner0 = randomVector3(lcg, 5.0f);
        auto corner1 = randomVector3(lcg, 5.0f);
        Box box(Vector3f::min(corner0, corner1), Vector3f::max(corner0, corner1));
        boxContains.push({box.contains(point) ? 1 : 0});
        boxCenter.push(box.center());
        boxExtents.push(box.extents());
        boxSurfaceArea.push({box.surfaceArea()});

        Ray ray;
        ray.origin = randomVector3(lcg, 5.0f);
        ray.direction = Vector3f::unitVector(randomVector3(lcg, 1.0f));

        Vector3f vertices[3];
        vertices[0] = randomVector3(lcg, 3.0f);
        vertices[1] = randomVector3(lcg, 3.0f);
        vertices[2] = randomVector3(lcg, 3.0f);
        Triangle triangle;
        triangle.indices[0] = 0;
        triangle.indices[1] = 1;
        triangle.indices[2] = 2;
        Mesh mesh(3, 1, vertices, &triangle);
        rayTriangle.push({ray.intersect(mesh, 0)});

        auto reciprocalDirection = Vector3f(Vector3f::reciprocal(ray.direction));
        int directionSigns[3];
        directionSigns[0] = (ray.direction.x() >= 0) ? 1 : 0;
        directionSigns[1] = (ray.direction.y() >= 0) ? 1 : 0;
        directionSigns[2] = (ray.direction.z() >= 0) ? 1 : 0;
        auto minDistance = 0.0f;
        auto maxDistance = std::numeric_limits<float>::infinity();
        auto hit = ray.intersect(box, reciprocalDirection, directionSigns, minDistance, maxDistance);
        rayBox.push({hit ? 1.0f : 0.0f, minDistance, maxDistance});

        raySphere.push({ray.intersect(a)});
    }

    boundingSphere.write(out / "bounding_sphere.npy");
    sphereContains.write(out / "sphere_contains.npy");
    boxContains.write(out / "box_contains.npy");
    boxCenter.write(out / "box_center.npy");
    boxExtents.write(out / "box_extents.npy");
    boxSurfaceArea.write(out / "box_surface_area.npy");
    rayTriangle.write(out / "ray_triangle.npy");
    rayBox.write(out / "ray_box.npy");
    raySphere.write(out / "ray_sphere.npy");
}

Register vector("vector", generateVector);
Register matrix("matrix", generateMatrix);
Register quaternion("quaternion", generateQuaternion);
Register coordinateSpace("coordinate_space", generateCoordinateSpace);
Register polar("polar_vector", generatePolar);
Register shapes("shapes", generateShapes);

}
