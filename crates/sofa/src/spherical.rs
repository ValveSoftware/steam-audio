//! Coordinate conversions (upstream `hrtf/spherical.c` and `hrtf/tools.c`).

use std::f64::consts::PI;
use std::hint::black_box;

use crate::{Array, Attribute, Hrtf};

/// `radius`: `sqrtf(powf(x, 2) + powf(y, 2) + powf(z, 2))`.
///
/// MSVC's `powf(x, 2)` is not always `x * x` (it differs by an ulp for about one value in 17,000), and LLVM folds a
/// constant exponent of 2 into a multiplication, so the exponent is hidden from the optimizer to call the C
/// library's `powf` as upstream does.
pub(crate) fn radius(c: &[f32; 3]) -> f32 {
    let two = black_box(2.0f32);
    (c[0].powf(two) + c[1].powf(two) + c[2].powf(two)).sqrt()
}

/// Cartesian to spherical coordinates (upstream `mysofa_c2s`): `[x, y, z]` in metres to `[azimuth, elevation,
/// radius]`, the azimuth in degrees in `[0, 360)`, counterclockwise from the x axis, and the elevation in degrees.
pub fn c2s(values: [f32; 3]) -> [f32; 3] {
    let [x, y, z] = values;
    let r = radius(&values);
    let theta = z.atan2((x * x + y * y).sqrt());
    let phi = y.atan2(x);
    // `phi * (180 / M_PI) + 360` and `theta * (180 / M_PI)` are `double` expressions rounded to `float`.
    [
        (f64::from(phi) * (180.0 / PI) + 360.0) as f32 % 360.0,
        (f64::from(theta) * (180.0 / PI)) as f32,
        r,
    ]
}

/// Spherical to Cartesian coordinates (upstream `mysofa_s2c`), the inverse of [`c2s`].
pub fn s2c(values: [f32; 3]) -> [f32; 3] {
    let phi = (f64::from(values[0]) * (PI / 180.0)) as f32;
    let theta = (f64::from(values[1]) * (PI / 180.0)) as f32;
    let r = values[2];
    let x = theta.cos() * r;
    [phi.cos() * x, phi.sin() * x, theta.sin() * r]
}

/// `changeAttribute`: sets the value of the first attribute in upstream's list order (the last in the file) named
/// `name` whose value is `value`, any value if `value` is `None`, or missing.
fn change_attribute(
    attributes: &mut [Attribute],
    name: &str,
    value: Option<&str>,
    new_value: &str,
) -> bool {
    let found = attributes.iter_mut().rev().find(|a| {
        a.name == name.as_bytes()
            && match (value, &a.value) {
                (Some(value), Some(current)) => current == value.as_bytes(),
                _ => true,
            }
    });
    match found {
        Some(attribute) => {
            attribute.value = Some(new_value.as_bytes().to_vec());
            true
        }
        None => false,
    }
}

/// `convertArray2`.
fn convert_to_cartesian(array: &mut Array) {
    if !change_attribute(
        &mut array.attributes,
        "Type",
        Some("spherical"),
        "cartesian",
    ) {
        return;
    }
    change_attribute(&mut array.attributes, "Units", None, "meter");
    if let Some(values) = &mut array.values {
        for triple in values.chunks_exact_mut(3) {
            let converted = s2c([triple[0], triple[1], triple[2]]);
            triple.copy_from_slice(&converted);
        }
    }
}

impl Hrtf {
    /// Converts the positions and directions given in spherical coordinates to Cartesian ones (upstream
    /// `mysofa_tocartesian`). An array counts as spherical when its `Type` attribute is `spherical` or has no value;
    /// its `Type` becomes `cartesian` and its `Units` (if any) `meter`.
    pub fn to_cartesian(&mut self) {
        convert_to_cartesian(&mut self.listener_view);
        convert_to_cartesian(&mut self.listener_up);
        convert_to_cartesian(&mut self.listener_position);
        convert_to_cartesian(&mut self.emitter_position);
        convert_to_cartesian(&mut self.receiver_position);
        convert_to_cartesian(&mut self.source_position);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// MSVC's `powf(x, 2)` rounds 0x1.157p46² up by an ulp; `radius` must call it rather than multiply.
    #[test]
    #[cfg(all(windows, target_env = "msvc"))]
    fn radius_uses_the_c_library_powf() {
        let x = f32::from_bits(0x568a_b800);
        assert_eq!(x * x, f32::from_bits(0x6d96_55c8));
        assert_eq!(x.powf(black_box(2.0)), f32::from_bits(0x6d96_55c9));
        let r = radius(&[x, x, 0.0]);
        assert_eq!(r, (x.powf(black_box(2.0)) * 2.0).sqrt());
        assert_ne!(r, (x * x * 2.0).sqrt());
    }
}
