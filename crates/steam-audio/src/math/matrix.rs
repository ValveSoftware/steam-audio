use std::ops::{Add, AddAssign, Div, DivAssign, Index, IndexMut, Mul, MulAssign, Sub, SubAssign};

use super::{Vector3, Vector4};

/// An `R`×`C` matrix stored in column-major order, like upstream `Matrix<T, R, C>`.
///
/// Indexing is `m[(row, col)]`. Upstream's default constructor leaves elements uninitialized; here every
/// constructor initializes them.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Matrix<const R: usize, const C: usize> {
    /// `columns[col][row]`.
    pub columns: [[f32; R]; C],
}

pub type Matrix2 = Matrix<2, 2>;
pub type Matrix3 = Matrix<3, 3>;
pub type Matrix4 = Matrix<4, 4>;

impl<const R: usize, const C: usize> Default for Matrix<R, C> {
    fn default() -> Self {
        Self::ZERO
    }
}

impl<const R: usize, const C: usize> Matrix<R, C> {
    pub const ZERO: Self = Self {
        columns: [[0.0; R]; C],
    };

    /// Builds a matrix from rows, i.e. in the order it is written on paper (upstream's initializer-list
    /// constructor).
    pub fn from_rows(rows: [[f32; C]; R]) -> Self {
        let mut out = Self::ZERO;
        for (i, row) in rows.iter().enumerate() {
            for (j, &x) in row.iter().enumerate() {
                out[(i, j)] = x;
            }
        }
        out
    }

    /// Builds a matrix from its column-major element array (upstream `elements`).
    pub fn from_column_major(elements: &[f32]) -> Self {
        assert_eq!(elements.len(), R * C, "expected {} elements", R * C);
        let mut out = Self::ZERO;
        for (j, column) in out.columns.iter_mut().enumerate() {
            column.copy_from_slice(&elements[j * R..(j + 1) * R]);
        }
        out
    }

    /// Elements in column-major order, as upstream stores them.
    pub fn column_major(&self) -> impl Iterator<Item = f32> + '_ {
        self.columns.iter().flatten().copied()
    }

    /// Upstream `transposedCopy`.
    pub fn transpose(&self) -> Matrix<C, R> {
        let mut out = Matrix::<C, R>::ZERO;
        for i in 0..C {
            for j in 0..R {
                out[(i, j)] = self[(j, i)];
            }
        }
        out
    }

    fn map(mut self, f: impl Fn(f32) -> f32) -> Self {
        self.columns.iter_mut().flatten().for_each(|x| *x = f(*x));
        self
    }

    fn zip(mut self, rhs: &Self, f: impl Fn(f32, f32) -> f32) -> Self {
        for (a, &b) in self
            .columns
            .iter_mut()
            .flatten()
            .zip(rhs.columns.iter().flatten())
        {
            *a = f(*a, b);
        }
        self
    }

    /// Matrix-vector product on raw arrays; typed versions are `Matrix3 * Vector3` and `Matrix4 * Vector4`.
    pub fn mul_array(&self, v: [f32; C]) -> [f32; R] {
        let mut out = [0.0; R];
        for (i, o) in out.iter_mut().enumerate() {
            for (j, &x) in v.iter().enumerate() {
                *o += self[(i, j)] * x;
            }
        }
        out
    }
}

impl<const N: usize> Matrix<N, N> {
    pub const IDENTITY: Self = {
        let mut out = Self::ZERO;
        let mut i = 0;
        while i < N {
            out.columns[i][i] = 1.0;
            i += 1;
        }
        out
    };
}

impl Matrix2 {
    pub fn determinant(&self) -> f32 {
        let m = self;
        m[(0, 0)] * m[(1, 1)] - m[(0, 1)] * m[(1, 0)]
    }
}

impl Matrix3 {
    /// Cofactor expansion along the first row, in upstream's operation order.
    pub fn determinant(&self) -> f32 {
        let m = self;
        let minor_efhi = m[(1, 1)] * m[(2, 2)] - m[(1, 2)] * m[(2, 1)];
        let minor_dfgi = m[(1, 0)] * m[(2, 2)] - m[(1, 2)] * m[(2, 0)];
        let minor_degh = m[(1, 0)] * m[(2, 1)] - m[(1, 1)] * m[(2, 0)];
        m[(0, 0)] * minor_efhi - m[(0, 1)] * minor_dfgi + m[(0, 2)] * minor_degh
    }
}

impl Matrix4 {
    /// Cofactor expansion along the first row, in upstream's operation order.
    pub fn determinant(&self) -> f32 {
        let m = self;
        let minor_klop = m[(2, 2)] * m[(3, 3)] - m[(2, 3)] * m[(3, 2)];
        // Upstream writes this minor with the second product's factors swapped; multiplication is
        // commutative, so the result is identical.
        let minor_jlnp = m[(2, 1)] * m[(3, 3)] - m[(3, 1)] * m[(2, 3)];
        let minor_jkno = m[(2, 1)] * m[(3, 2)] - m[(2, 2)] * m[(3, 1)];
        let minor_ilmp = m[(2, 0)] * m[(3, 3)] - m[(2, 3)] * m[(3, 0)];
        let minor_ikmo = m[(2, 0)] * m[(3, 2)] - m[(2, 2)] * m[(3, 0)];
        let minor_ijmn = m[(2, 0)] * m[(3, 1)] - m[(2, 1)] * m[(3, 0)];

        let minor_fgh = m[(1, 1)] * minor_klop - m[(1, 2)] * minor_jlnp + m[(1, 3)] * minor_jkno;
        let minor_egh = m[(1, 0)] * minor_klop - m[(1, 2)] * minor_ilmp + m[(1, 3)] * minor_ikmo;
        let minor_efh = m[(1, 0)] * minor_jlnp - m[(1, 1)] * minor_ilmp + m[(1, 3)] * minor_ijmn;
        let minor_efg = m[(1, 0)] * minor_jkno - m[(1, 1)] * minor_ikmo + m[(1, 2)] * minor_ijmn;

        m[(0, 0)] * minor_fgh - m[(0, 1)] * minor_egh + m[(0, 2)] * minor_efh
            - m[(0, 3)] * minor_efg
    }

    /// Inverse via the adjugate, or the zero matrix if the determinant is exactly zero (upstream `inverse`).
    pub fn inverse_or_zero(&self) -> Self {
        let m = self;
        let klop = m[(2, 2)] * m[(3, 3)] - m[(2, 3)] * m[(3, 2)];
        let jlnp = m[(2, 1)] * m[(3, 3)] - m[(2, 3)] * m[(3, 1)];
        let jkno = m[(2, 1)] * m[(3, 2)] - m[(2, 2)] * m[(3, 1)];
        let ilmp = m[(2, 0)] * m[(3, 3)] - m[(2, 3)] * m[(3, 0)];
        let ikmo = m[(2, 0)] * m[(3, 2)] - m[(2, 2)] * m[(3, 0)];
        let ijmn = m[(2, 0)] * m[(3, 1)] - m[(2, 1)] * m[(3, 0)];
        let ghop = m[(1, 2)] * m[(3, 3)] - m[(1, 3)] * m[(3, 2)];
        let fhnp = m[(1, 1)] * m[(3, 3)] - m[(1, 3)] * m[(3, 1)];
        let fgno = m[(1, 1)] * m[(3, 2)] - m[(1, 2)] * m[(3, 1)];
        let ehmp = m[(1, 0)] * m[(3, 3)] - m[(1, 3)] * m[(3, 0)];
        let egmo = m[(1, 0)] * m[(3, 2)] - m[(1, 2)] * m[(3, 0)];
        let efmn = m[(1, 0)] * m[(3, 1)] - m[(1, 1)] * m[(3, 0)];
        let ghkl = m[(1, 2)] * m[(2, 3)] - m[(1, 3)] * m[(2, 2)];
        let fhjl = m[(1, 1)] * m[(2, 3)] - m[(1, 3)] * m[(2, 1)];
        let fgjk = m[(1, 1)] * m[(2, 2)] - m[(1, 2)] * m[(2, 1)];
        let ehil = m[(1, 0)] * m[(2, 3)] - m[(1, 3)] * m[(2, 0)];
        let egik = m[(1, 0)] * m[(2, 2)] - m[(1, 2)] * m[(2, 0)];
        let efij = m[(1, 0)] * m[(2, 1)] - m[(1, 1)] * m[(2, 0)];

        let d_11 = m[(1, 1)] * klop - m[(1, 2)] * jlnp + m[(1, 3)] * jkno;
        let d_12 = m[(1, 0)] * klop - m[(1, 2)] * ilmp + m[(1, 3)] * ikmo;
        let d_13 = m[(1, 0)] * jlnp - m[(1, 1)] * ilmp + m[(1, 3)] * ijmn;
        let d_14 = m[(1, 0)] * jkno - m[(1, 1)] * ikmo + m[(1, 2)] * ijmn;
        let d_21 = m[(0, 1)] * klop - m[(0, 2)] * jlnp + m[(0, 3)] * jkno;
        let d_22 = m[(0, 0)] * klop - m[(0, 2)] * ilmp + m[(0, 3)] * ikmo;
        let d_23 = m[(0, 0)] * jlnp - m[(0, 1)] * ilmp + m[(0, 3)] * ijmn;
        let d_24 = m[(0, 0)] * jkno - m[(0, 1)] * ikmo + m[(0, 2)] * ijmn;
        let d_31 = m[(0, 1)] * ghop - m[(0, 2)] * fhnp + m[(0, 3)] * fgno;
        let d_32 = m[(0, 0)] * ghop - m[(0, 2)] * ehmp + m[(0, 3)] * egmo;
        let d_33 = m[(0, 0)] * fhnp - m[(0, 1)] * ehmp + m[(0, 3)] * efmn;
        let d_34 = m[(0, 0)] * fgno - m[(0, 1)] * egmo + m[(0, 2)] * efmn;
        let d_41 = m[(0, 1)] * ghkl - m[(0, 2)] * fhjl + m[(0, 3)] * fgjk;
        let d_42 = m[(0, 0)] * ghkl - m[(0, 2)] * ehil + m[(0, 3)] * egik;
        let d_43 = m[(0, 0)] * fhjl - m[(0, 1)] * ehil + m[(0, 3)] * efij;
        let d_44 = m[(0, 0)] * fgjk - m[(0, 1)] * egik + m[(0, 2)] * efij;

        let det = m[(0, 0)] * d_11 - m[(0, 1)] * d_12 + m[(0, 2)] * d_13 - m[(0, 3)] * d_14;
        if det == 0.0 {
            return Self::ZERO;
        }

        Self::from_rows([
            [d_11, -d_21, d_31, -d_41],
            [-d_12, d_22, -d_32, d_42],
            [d_13, -d_23, d_33, -d_43],
            [-d_14, d_24, -d_34, d_44],
        ]) / det
    }
}

impl<const R: usize, const C: usize> Index<(usize, usize)> for Matrix<R, C> {
    type Output = f32;

    fn index(&self, (row, col): (usize, usize)) -> &f32 {
        &self.columns[col][row]
    }
}

impl<const R: usize, const C: usize> IndexMut<(usize, usize)> for Matrix<R, C> {
    fn index_mut(&mut self, (row, col): (usize, usize)) -> &mut f32 {
        &mut self.columns[col][row]
    }
}

macro_rules! matrix_assign_ops {
    ($($op:ident, $method:ident, $assign_op:ident, $assign_method:ident, $f:expr;)+) => {$(
        impl<const R: usize, const C: usize> $assign_op for Matrix<R, C> {
            fn $assign_method(&mut self, rhs: Self) {
                *self = self.zip(&rhs, $f);
            }
        }

        impl<const R: usize, const C: usize> $op for Matrix<R, C> {
            type Output = Self;

            fn $method(self, rhs: Self) -> Self {
                self.zip(&rhs, $f)
            }
        }
    )+};
}

matrix_assign_ops! {
    Add, add, AddAssign, add_assign, |a, b| a + b;
    Sub, sub, SubAssign, sub_assign, |a, b| a - b;
}

macro_rules! matrix_scalar_ops {
    ($($op:ident, $method:ident, $assign_op:ident, $assign_method:ident;)+) => {$(
        impl<const R: usize, const C: usize> $assign_op<f32> for Matrix<R, C> {
            fn $assign_method(&mut self, s: f32) {
                *self = $op::$method(*self, s);
            }
        }
    )+};
}

matrix_scalar_ops! {
    Add, add, AddAssign, add_assign;
    Sub, sub, SubAssign, sub_assign;
    Mul, mul, MulAssign, mul_assign;
    Div, div, DivAssign, div_assign;
}

/// Adds `s` to every element.
impl<const R: usize, const C: usize> Add<f32> for Matrix<R, C> {
    type Output = Self;

    fn add(self, s: f32) -> Self {
        self.map(|x| x + s)
    }
}

/// Subtracts `s` from every element.
impl<const R: usize, const C: usize> Sub<f32> for Matrix<R, C> {
    type Output = Self;

    fn sub(self, s: f32) -> Self {
        self.map(|x| x - s)
    }
}

impl<const R: usize, const C: usize> Mul<f32> for Matrix<R, C> {
    type Output = Self;

    fn mul(self, s: f32) -> Self {
        self.map(|x| x * s)
    }
}

/// Computed as multiplication by `1 / s`, like upstream.
impl<const R: usize, const C: usize> Div<f32> for Matrix<R, C> {
    type Output = Self;

    fn div(self, s: f32) -> Self {
        debug_assert!(s != 0.0, "division of a matrix by zero");
        let r = 1.0 / s;
        self.map(|x| x * r)
    }
}

/// Matrix product, accumulating each element from zero in `k` order like upstream.
impl<const R: usize, const C: usize, const C2: usize> Mul<Matrix<C, C2>> for Matrix<R, C> {
    type Output = Matrix<R, C2>;

    fn mul(self, rhs: Matrix<C, C2>) -> Matrix<R, C2> {
        let mut out = Matrix::<R, C2>::ZERO;
        for i in 0..R {
            for j in 0..C2 {
                for k in 0..C {
                    out[(i, j)] += self[(i, k)] * rhs[(k, j)];
                }
            }
        }
        out
    }
}

impl Mul<Vector3> for Matrix3 {
    type Output = Vector3;

    fn mul(self, v: Vector3) -> Vector3 {
        Vector3::from_array(self.mul_array(v.to_array()))
    }
}

impl Mul<Vector4> for Matrix4 {
    type Output = Vector4;

    fn mul(self, v: Vector4) -> Vector4 {
        Vector4::from_array(self.mul_array(v.to_array()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn layout_is_column_major() {
        let m = Matrix::<2, 3>::from_rows([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]);
        assert_eq!(
            m.column_major().collect::<Vec<_>>(),
            [1.0, 4.0, 2.0, 5.0, 3.0, 6.0]
        );
        assert_eq!(m[(1, 2)], 6.0);
        assert_eq!(m.transpose()[(2, 1)], 6.0);
    }

    #[test]
    fn upstream_determinants() {
        // Values from core/src/test/Matrix.test.cpp.
        let m2 = Matrix2::from_rows([[-3.0, 7.0], [-1.0, -11.0]]);
        assert_eq!(m2.determinant(), 40.0);
        let m4 = Matrix4::IDENTITY * 2.0;
        assert_eq!(m4.determinant(), 16.0);
    }

    #[test]
    fn singular_inverse_is_zero() {
        let m = Matrix4::from_rows([[1.0, 2.0, 3.0, 4.0]; 4]);
        assert_eq!(m.inverse_or_zero(), Matrix4::ZERO);
    }

    #[test]
    fn inverse_of_identity() {
        assert_eq!(Matrix4::IDENTITY.inverse_or_zero(), Matrix4::IDENTITY);
    }

    #[test]
    fn product_with_identity() {
        let m = Matrix3::from_rows([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]]);
        assert_eq!(m * Matrix3::IDENTITY, m);
        assert_eq!(m * Vector3::X_AXIS, Vector3::new(1.0, 4.0, 7.0));
    }
}
