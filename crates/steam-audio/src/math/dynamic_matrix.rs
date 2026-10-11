use std::ops::{Index, IndexMut};

/// A heap-allocated `rows`×`cols` matrix in column-major order (upstream `DynamicMatrix<T>`).
///
/// The free functions of upstream (`addMatrices`, `multiplyMatrices`, ...) become associated functions that write
/// into a caller-provided output, so they can be used without allocating.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct DynamicMatrix {
    rows: usize,
    cols: usize,
    elements: Vec<f32>,
}

impl DynamicMatrix {
    /// A zero-filled matrix.
    pub fn new(rows: usize, cols: usize) -> Self {
        Self {
            rows,
            cols,
            elements: vec![0.0; rows * cols],
        }
    }

    /// From rows written as on paper; all rows must have the same length.
    pub fn from_rows<R: AsRef<[f32]>>(rows: &[R]) -> Self {
        let cols = rows.first().map_or(0, |r| r.as_ref().len());
        let mut out = Self::new(rows.len(), cols);
        for (i, row) in rows.iter().enumerate() {
            let row = row.as_ref();
            assert_eq!(
                row.len(),
                cols,
                "row {i} has {} elements, expected {cols}",
                row.len()
            );
            for (j, &x) in row.iter().enumerate() {
                out[(i, j)] = x;
            }
        }
        out
    }

    /// From a column-major element buffer.
    pub fn from_column_major(rows: usize, cols: usize, elements: Vec<f32>) -> Self {
        assert_eq!(
            elements.len(),
            rows * cols,
            "expected {} elements",
            rows * cols
        );
        Self {
            rows,
            cols,
            elements,
        }
    }

    pub fn rows(&self) -> usize {
        self.rows
    }

    pub fn cols(&self) -> usize {
        self.cols
    }

    /// Elements in column-major order.
    pub fn as_slice(&self) -> &[f32] {
        &self.elements
    }

    pub fn as_mut_slice(&mut self) -> &mut [f32] {
        &mut self.elements
    }

    pub fn fill_zero(&mut self) {
        self.elements.fill(0.0);
    }

    /// Resizes and zero-fills (upstream `resize`).
    pub fn resize(&mut self, rows: usize, cols: usize) {
        self.rows = rows;
        self.cols = cols;
        self.elements.clear();
        self.elements.resize(rows * cols, 0.0);
    }

    fn assert_same_shape(&self, other: &Self) {
        assert!(
            self.rows == other.rows && self.cols == other.cols,
            "shape mismatch: {}x{} vs {}x{}",
            self.rows,
            self.cols,
            other.rows,
            other.cols
        );
    }

    /// `out = a + b` (upstream `addMatrices`).
    pub fn add(a: &Self, b: &Self, out: &mut Self) {
        a.assert_same_shape(out);
        b.assert_same_shape(out);
        for ((o, &x), &y) in out.elements.iter_mut().zip(&a.elements).zip(&b.elements) {
            *o = x + y;
        }
    }

    /// `out = a - b` (upstream `subtractMatrices`).
    pub fn subtract(a: &Self, b: &Self, out: &mut Self) {
        a.assert_same_shape(out);
        b.assert_same_shape(out);
        for ((o, &x), &y) in out.elements.iter_mut().zip(&a.elements).zip(&b.elements) {
            *o = x - y;
        }
    }

    /// `out = s * a` (upstream `scaleMatrix`).
    pub fn scale(a: &Self, s: f32, out: &mut Self) {
        a.assert_same_shape(out);
        for (o, &x) in out.elements.iter_mut().zip(&a.elements) {
            *o = s * x;
        }
    }

    /// `out = a * b`, accumulating each element from zero in `k` order (upstream `multiplyMatrices`, non-MKL
    /// path).
    pub fn multiply(a: &Self, b: &Self, out: &mut Self) {
        assert!(
            a.rows == out.rows && b.cols == out.cols && a.cols == b.rows,
            "cannot multiply {}x{} by {}x{} into {}x{}",
            a.rows,
            a.cols,
            b.rows,
            b.cols,
            out.rows,
            out.cols
        );
        out.fill_zero();
        for j in 0..out.cols {
            let b_col = &b.elements[j * b.rows..(j + 1) * b.rows];
            for i in 0..out.rows {
                let mut acc = 0.0;
                for (k, &bk) in b_col.iter().enumerate() {
                    acc += a.elements[k * a.rows + i] * bk;
                }
                out.elements[j * out.rows + i] = acc;
            }
        }
    }

    /// `out = m * v` (upstream `multiplyMatrixVector`).
    pub fn multiply_vector(m: &Self, v: &[f32], out: &mut [f32]) {
        assert_eq!(v.len(), m.cols, "vector length");
        assert_eq!(out.len(), m.rows, "output length");
        for (i, o) in out.iter_mut().enumerate() {
            *o = 0.0;
            for (j, &x) in v.iter().enumerate() {
                *o += m[(i, j)] * x;
            }
        }
    }
}

impl Index<(usize, usize)> for DynamicMatrix {
    type Output = f32;

    fn index(&self, (row, col): (usize, usize)) -> &f32 {
        debug_assert!(
            row < self.rows && col < self.cols,
            "({row}, {col}) out of range"
        );
        &self.elements[col * self.rows + row]
    }
}

impl IndexMut<(usize, usize)> for DynamicMatrix {
    fn index_mut(&mut self, (row, col): (usize, usize)) -> &mut f32 {
        debug_assert!(
            row < self.rows && col < self.cols,
            "({row}, {col}) out of range"
        );
        &mut self.elements[col * self.rows + row]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_is_empty() {
        let m = DynamicMatrix::default();
        assert_eq!((m.rows(), m.cols()), (0, 0));
        assert!(m.as_slice().is_empty());
    }

    #[test]
    fn from_rows_is_column_major() {
        let m = DynamicMatrix::from_rows(&[[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]);
        assert_eq!(m.as_slice(), [1.0, 4.0, 2.0, 5.0, 3.0, 6.0]);
        assert_eq!(m[(1, 2)], 6.0);
    }

    #[test]
    fn multiply_small() {
        let a = DynamicMatrix::from_rows(&[[1.0, 2.0], [3.0, 4.0]]);
        let b = DynamicMatrix::from_rows(&[[5.0], [6.0]]);
        let mut c = DynamicMatrix::new(2, 1);
        DynamicMatrix::multiply(&a, &b, &mut c);
        assert_eq!(c.as_slice(), [17.0, 39.0]);
    }

    #[test]
    #[should_panic(expected = "cannot multiply")]
    fn multiply_shape_mismatch_panics() {
        let a = DynamicMatrix::new(2, 3);
        let mut c = DynamicMatrix::new(2, 2);
        DynamicMatrix::multiply(&a, &a, &mut c);
    }
}
