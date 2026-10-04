use crate::{Error, Result};
/// Parse decimal/scientific notation exactly; never round a payment.
pub fn parse_amount(text: &str, decimals: u32) -> Result<u128> {
    let bad = || Error::InvalidAmount("expected positive, exact 128-bit atomic units");
    if text.is_empty() || text.len() > 128 || decimals > 18 {
        return Err(bad());
    }
    let (mantissa, exponent) = match text.find(['e', 'E']) {
        Some(i) => (&text[..i], text[i + 1..].parse::<i32>().map_err(|_| bad())?),
        None => (text, 0),
    };
    if !(-128..=128).contains(&exponent) {
        return Err(bad());
    }
    let point = mantissa.find(['.', ',']);
    let fraction = point.map_or(0, |i| mantissa.len() - i - 1) as i32;
    let mut digits = mantissa.to_owned();
    if let Some(i) = point {
        digits.remove(i);
    }
    if digits.is_empty() || !digits.bytes().all(|c| c.is_ascii_digit()) {
        return Err(bad());
    }
    digits = digits.trim_start_matches('0').to_owned();
    if digits.is_empty() {
        return Err(bad());
    }
    let power = decimals as i32 + exponent - fraction;
    if power < 0 {
        let remove = (-power) as usize;
        if remove >= digits.len() || !digits[digits.len() - remove..].bytes().all(|b| b == b'0') {
            return Err(bad());
        }
        digits.truncate(digits.len() - remove);
    } else {
        if digits.len() + power as usize > 39 {
            return Err(bad());
        }
        digits.extend(std::iter::repeat_n('0', power as usize));
    }
    digits.parse().map_err(|_| bad())
}
pub fn format_amount(amount: u128, decimals: u32) -> Result<String> {
    if decimals > 18 {
        return Err(Error::InvalidArgument("invalid currency decimals".into()));
    }
    let mut text = amount.to_string();
    if decimals == 0 {
        return Ok(text);
    }
    let d = decimals as usize;
    if text.len() <= d {
        text = format!("{}{}", "0".repeat(d + 1 - text.len()), text);
    }
    text.insert(text.len() - d, '.');
    Ok(text.trim_end_matches('0').trim_end_matches('.').into())
}
