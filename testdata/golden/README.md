# Golden data

Reference outputs of the original C++ Steam Audio, written by `tools/golden` as NumPy `.npy` files
(`<module>/<case>.npy`, little-endian, C order). Rust tests read them with `parity::golden`.

Do not edit these files by hand; regenerate them from the reference build instead.
