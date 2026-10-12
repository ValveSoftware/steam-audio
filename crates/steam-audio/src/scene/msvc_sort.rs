//! `std::sort` exactly as MSVC's STL implements it (`<algorithm>`, `__msvc_heap_algorithms.hpp`, VS 2022 17.14).
//!
//! `std::sort` is not stable, and which of several equal elements ends up where is implementation-defined. The BVH
//! builder sorts triangle centroids, and axis-aligned meshes have many equal coordinates, so the tree's layout
//! depends on this order. Porting MSVC's introsort (insertion sort below 33 elements, median-of-three or Tukey's
//! ninther partitioning into less / equal / greater, heap sort after 1.5 log2(n) levels) lets the port build the
//! same tree as the reference build. MSVC's STL is Apache-2.0 WITH LLVM-exception.

const INSERTION_SORT_MAX: usize = 32;

/// Sorts `v` with the strict weak ordering `less`, leaving equal elements in the order MSVC's `std::sort` would.
pub(crate) fn sort<T: Copy>(v: &mut [T], less: impl Fn(&T, &T) -> bool + Copy) {
    let n = v.len();
    sort_range(v, 0, n, n as isize, less);
}

/// `_Sort_unchecked`: orders `v[first..last]`, falling back to heap sort once `ideal` runs out.
fn sort_range<T: Copy>(
    v: &mut [T],
    mut first: usize,
    mut last: usize,
    mut ideal: isize,
    less: impl Fn(&T, &T) -> bool + Copy,
) {
    loop {
        if last - first <= INSERTION_SORT_MAX {
            insertion_sort(&mut v[first..last], less);
            return;
        }

        if ideal <= 0 {
            make_heap(&mut v[first..last], less);
            sort_heap(&mut v[first..last], less);
            return;
        }

        let (mid_first, mid_last) = partition_by_median_guess(v, first, last, less);

        ideal = (ideal >> 1) + (ideal >> 2);

        if mid_first - first < last - mid_last {
            sort_range(v, first, mid_first, ideal, less);
            first = mid_last;
        } else {
            sort_range(v, mid_last, last, ideal, less);
            last = mid_first;
        }
    }
}

fn insertion_sort<T: Copy>(v: &mut [T], less: impl Fn(&T, &T) -> bool) {
    for mid in 1..v.len() {
        let value = v[mid];
        if less(&value, &v[0]) {
            v.copy_within(0..mid, 1);
            v[0] = value;
        } else {
            let mut hole = mid;
            while less(&value, &v[hole - 1]) {
                v[hole] = v[hole - 1];
                hole -= 1;
            }
            v[hole] = value;
        }
    }
}

/// `_Med3_unchecked`: sorts the elements at `a`, `b` and `c` so that the median is at `b`.
fn median_of_three<T: Copy>(
    v: &mut [T],
    a: usize,
    b: usize,
    c: usize,
    less: impl Fn(&T, &T) -> bool,
) {
    if less(&v[b], &v[a]) {
        v.swap(b, a);
    }

    if less(&v[c], &v[b]) {
        v.swap(c, b);

        if less(&v[b], &v[a]) {
            v.swap(b, a);
        }
    }
}

/// `_Guess_median_unchecked`; `last` is inclusive.
fn guess_median<T: Copy>(
    v: &mut [T],
    first: usize,
    mid: usize,
    last: usize,
    less: impl Fn(&T, &T) -> bool + Copy,
) {
    let count = last - first;
    if 40 < count {
        let step = (count + 1) >> 3;
        let two_step = step << 1;
        median_of_three(v, first, first + step, first + two_step, less);
        median_of_three(v, mid - step, mid, mid + step, less);
        median_of_three(v, last - two_step, last - step, last, less);
        median_of_three(v, first + step, mid, last - step, less);
    } else {
        median_of_three(v, first, mid, last, less);
    }
}

/// `_Partition_by_median_guess_unchecked`: partitions `v[first..last]` into elements less than, equal to and
/// greater than a pivot, and returns the range of the equal ones.
fn partition_by_median_guess<T: Copy>(
    v: &mut [T],
    first: usize,
    last: usize,
    less: impl Fn(&T, &T) -> bool + Copy,
) -> (usize, usize) {
    let mid = first + ((last - first) >> 1);
    guess_median(v, first, mid, last - 1, less);
    let mut p_first = mid;
    let mut p_last = p_first + 1;

    while first < p_first
        && !less(&v[p_first - 1], &v[p_first])
        && !less(&v[p_first], &v[p_first - 1])
    {
        p_first -= 1;
    }

    while p_last < last && !less(&v[p_last], &v[p_first]) && !less(&v[p_first], &v[p_last]) {
        p_last += 1;
    }

    let mut g_first = p_last;
    let mut g_last = p_first;

    loop {
        while g_first < last {
            if less(&v[p_first], &v[g_first]) {
                // Greater: stays where it is.
            } else if less(&v[g_first], &v[p_first]) {
                break;
            } else if p_last != g_first {
                v.swap(p_last, g_first);
                p_last += 1;
            } else {
                p_last += 1;
            }
            g_first += 1;
        }

        while first < g_last {
            let g_last_prev = g_last - 1;
            if less(&v[g_last_prev], &v[p_first]) {
                // Less: stays where it is.
            } else if less(&v[p_first], &v[g_last_prev]) {
                break;
            } else {
                p_first -= 1;
                if p_first != g_last_prev {
                    v.swap(p_first, g_last_prev);
                }
            }
            g_last -= 1;
        }

        if g_last == first && g_first == last {
            return (p_first, p_last);
        }

        if g_last == first {
            // No room at the bottom: rotate the pivot upwards.
            if p_last != g_first {
                v.swap(p_first, p_last);
            }

            p_last += 1;
            v.swap(p_first, g_first);
            p_first += 1;
            g_first += 1;
        } else if g_first == last {
            // No room at the top: rotate the pivot downwards.
            g_last -= 1;
            p_first -= 1;
            if g_last != p_first {
                v.swap(g_last, p_first);
            }

            p_last -= 1;
            v.swap(p_first, p_last);
        } else {
            g_last -= 1;
            v.swap(g_first, g_last);
            g_first += 1;
        }
    }
}

/// `_Push_heap_by_index`: moves the hole up towards `top` until `value` fits, then drops it in.
fn push_heap_by_index<T: Copy>(
    v: &mut [T],
    mut hole: usize,
    top: usize,
    value: T,
    less: impl Fn(&T, &T) -> bool,
) {
    while top < hole {
        let parent = (hole - 1) >> 1;
        if !less(&v[parent], &value) {
            break;
        }
        v[hole] = v[parent];
        hole = parent;
    }

    v[hole] = value;
}

/// `_Pop_heap_hole_by_index`: moves the hole down to the bottom along the larger children, then pushes `value`.
fn pop_heap_hole_by_index<T: Copy>(
    v: &mut [T],
    mut hole: usize,
    bottom: usize,
    value: T,
    less: impl Fn(&T, &T) -> bool + Copy,
) {
    let top = hole;
    let mut index = hole;

    let max_non_leaf = (bottom - 1) >> 1;
    while index < max_non_leaf {
        index = 2 * index + 2;
        if less(&v[index], &v[index - 1]) {
            index -= 1;
        }
        v[hole] = v[index];
        hole = index;
    }

    if index == max_non_leaf && bottom % 2 == 0 {
        v[hole] = v[bottom - 1];
        hole = bottom - 1;
    }

    push_heap_by_index(v, hole, top, value, less);
}

fn make_heap<T: Copy>(v: &mut [T], less: impl Fn(&T, &T) -> bool + Copy) {
    let bottom = v.len();
    let mut hole = bottom >> 1;
    while hole > 0 {
        hole -= 1;
        let value = v[hole];
        pop_heap_hole_by_index(v, hole, bottom, value, less);
    }
}

fn sort_heap<T: Copy>(v: &mut [T], less: impl Fn(&T, &T) -> bool + Copy) {
    let mut last = v.len();
    while last >= 2 {
        // `_Pop_heap_unchecked`: moves the top to `last - 1` and reheaps the rest.
        last -= 1;
        let value = v[last];
        v[last] = v[0];
        pop_heap_hole_by_index(v, 0, last, value, less);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Small LCG so the tests need no dependency.
    fn values(n: usize, distinct: u32, seed: u32) -> Vec<(u32, usize)> {
        let mut state = seed;
        (0..n)
            .map(|i| {
                state = state.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
                ((state >> 8) % distinct, i)
            })
            .collect()
    }

    #[test]
    fn sorts_by_key() {
        for n in [0, 1, 2, 31, 32, 33, 40, 41, 100, 1000, 5000] {
            for distinct in [1, 2, 7, 1 << 20] {
                let mut v = values(n, distinct, n as u32 + distinct);
                let mut expected = v.clone();
                sort(&mut v, |a, b| a.0 < b.0);
                expected.sort_by_key(|x| x.0);
                assert_eq!(
                    v.iter().map(|x| x.0).collect::<Vec<_>>(),
                    expected.iter().map(|x| x.0).collect::<Vec<_>>()
                );
                // A permutation of the input.
                let mut indices: Vec<_> = v.iter().map(|x| x.1).collect();
                indices.sort_unstable();
                assert!(indices.iter().copied().eq(0..n));
            }
        }
    }

    #[test]
    fn heap_sort_fallback_sorts() {
        let mut v = values(500, 50, 3);
        let mut expected = v.clone();
        make_heap(&mut v, |a, b| a.0 < b.0);
        sort_heap(&mut v, |a, b| a.0 < b.0);
        expected.sort_by_key(|x| x.0);
        assert!(v.iter().map(|x| x.0).eq(expected.iter().map(|x| x.0)));
    }
}
