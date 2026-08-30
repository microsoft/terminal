/// Portable GUID value using the same field layout as the Windows `GUID` type.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct Guid {
    pub data1: u32,
    pub data2: u16,
    pub data3: u16,
    pub data4: [u8; 8],
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct GuidParseError;

impl Guid {
    #[must_use]
    pub const fn new(data1: u32, data2: u16, data3: u16, data4: [u8; 8]) -> Self {
        Self {
            data1,
            data2,
            data3,
            data4,
        }
    }

    #[must_use]
    pub fn to_braced_string(self) -> String {
        format!(
            "{{{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}}}",
            self.data1,
            self.data2,
            self.data3,
            self.data4[0],
            self.data4[1],
            self.data4[2],
            self.data4[3],
            self.data4[4],
            self.data4[5],
            self.data4[6],
            self.data4[7]
        )
    }

    #[must_use]
    pub fn to_plain_string(self) -> String {
        self.to_braced_string()[1..37].to_owned()
    }

    /// Parses a braced or plain GUID string.
    ///
    /// # Errors
    ///
    /// Returns [`GuidParseError`] when the input is not a 36-character GUID,
    /// optionally enclosed in braces, with valid hexadecimal fields.
    pub fn parse(input: &str) -> Result<Self, GuidParseError> {
        let body = match (input.strip_prefix('{'), input.strip_suffix('}')) {
            (Some(without_open), Some(_)) if input.len() == 38 => &without_open[..36],
            _ if input.len() == 36 => input,
            _ => return Err(GuidParseError),
        };
        if body.as_bytes().get(8) != Some(&b'-')
            || body.as_bytes().get(13) != Some(&b'-')
            || body.as_bytes().get(18) != Some(&b'-')
            || body.as_bytes().get(23) != Some(&b'-')
        {
            return Err(GuidParseError);
        }
        let hex = |range: std::ops::Range<usize>| {
            u64::from_str_radix(&body[range], 16).map_err(|_| GuidParseError)
        };
        let data1 = u32::try_from(hex(0..8)?).map_err(|_| GuidParseError)?;
        let data2 = u16::try_from(hex(9..13)?).map_err(|_| GuidParseError)?;
        let data3 = u16::try_from(hex(14..18)?).map_err(|_| GuidParseError)?;
        let data4 = [
            u8::try_from(hex(19..21)?).map_err(|_| GuidParseError)?,
            u8::try_from(hex(21..23)?).map_err(|_| GuidParseError)?,
            u8::try_from(hex(24..26)?).map_err(|_| GuidParseError)?,
            u8::try_from(hex(26..28)?).map_err(|_| GuidParseError)?,
            u8::try_from(hex(28..30)?).map_err(|_| GuidParseError)?,
            u8::try_from(hex(30..32)?).map_err(|_| GuidParseError)?,
            u8::try_from(hex(32..34)?).map_err(|_| GuidParseError)?,
            u8::try_from(hex(34..36)?).map_err(|_| GuidParseError)?,
        ];
        Ok(Self::new(data1, data2, data3, data4))
    }

    fn network_bytes(self) -> [u8; 16] {
        let mut bytes = [0; 16];
        bytes[0..4].copy_from_slice(&self.data1.to_be_bytes());
        bytes[4..6].copy_from_slice(&self.data2.to_be_bytes());
        bytes[6..8].copy_from_slice(&self.data3.to_be_bytes());
        bytes[8..16].copy_from_slice(&self.data4);
        bytes
    }

    fn from_network_bytes(bytes: [u8; 16]) -> Self {
        Self {
            data1: u32::from_be_bytes(bytes[0..4].try_into().expect("four-byte GUID field")),
            data2: u16::from_be_bytes(bytes[4..6].try_into().expect("two-byte GUID field")),
            data3: u16::from_be_bytes(bytes[6..8].try_into().expect("two-byte GUID field")),
            data4: bytes[8..16].try_into().expect("eight-byte GUID field"),
        }
    }
}

#[must_use]
pub fn create_v5_uuid(namespace: Guid, name: &[u8]) -> Guid {
    let mut input = Vec::with_capacity(16 + name.len());
    input.extend_from_slice(&namespace.network_bytes());
    input.extend_from_slice(name);
    let digest = sha1(&input);
    let mut bytes = [0u8; 16];
    bytes.copy_from_slice(&digest[..16]);
    bytes[6] = (bytes[6] & 0x0f) | 0x50;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    Guid::from_network_bytes(bytes)
}

fn sha1(input: &[u8]) -> [u8; 20] {
    let bit_len = (input.len() as u64).wrapping_mul(8);
    let mut message = input.to_vec();
    message.push(0x80);
    while message.len() % 64 != 56 {
        message.push(0);
    }
    message.extend_from_slice(&bit_len.to_be_bytes());

    let mut h0 = 0x6745_2301u32;
    let mut h1 = 0xefcd_ab89u32;
    let mut h2 = 0x98ba_dcfeu32;
    let mut h3 = 0x1032_5476u32;
    let mut h4 = 0xc3d2_e1f0u32;

    for chunk in message.as_chunks::<64>().0 {
        let mut words = [0u32; 80];
        for (index, word) in words[..16].iter_mut().enumerate() {
            let start = index * 4;
            *word = u32::from_be_bytes(chunk[start..start + 4].try_into().expect("SHA-1 word"));
        }
        for index in 16..80 {
            words[index] =
                (words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16])
                    .rotate_left(1);
        }

        let mut work_a = h0;
        let mut work_b = h1;
        let mut work_c = h2;
        let mut work_d = h3;
        let mut work_e = h4;
        for (index, word) in words.into_iter().enumerate() {
            let (round_fn, round_constant) = match index {
                0..=19 => ((work_b & work_c) | ((!work_b) & work_d), 0x5a82_7999),
                20..=39 => (work_b ^ work_c ^ work_d, 0x6ed9_eba1),
                40..=59 => (
                    (work_b & work_c) | (work_b & work_d) | (work_c & work_d),
                    0x8f1b_bcdc,
                ),
                _ => (work_b ^ work_c ^ work_d, 0xca62_c1d6),
            };
            let temp = work_a
                .rotate_left(5)
                .wrapping_add(round_fn)
                .wrapping_add(work_e)
                .wrapping_add(round_constant)
                .wrapping_add(word);
            work_e = work_d;
            work_d = work_c;
            work_c = work_b.rotate_left(30);
            work_b = work_a;
            work_a = temp;
        }

        h0 = h0.wrapping_add(work_a);
        h1 = h1.wrapping_add(work_b);
        h2 = h2.wrapping_add(work_c);
        h3 = h3.wrapping_add(work_d);
        h4 = h4.wrapping_add(work_e);
    }

    let mut digest = [0u8; 20];
    for (chunk, word) in digest
        .as_chunks_mut::<4>()
        .0
        .iter_mut()
        .zip([h0, h1, h2, h3, h4])
    {
        chunk.copy_from_slice(&word.to_be_bytes());
    }
    digest
}

#[cfg(test)]
mod tests {
    use super::{Guid, create_v5_uuid};

    const TEST_NAMESPACE_GUID: Guid = Guid::new(
        0xad56_de9e,
        0x5167,
        0x41b6,
        [0x80, 0xeb, 0xfb, 0x19, 0xf7, 0x92, 0x7d, 0x1a],
    );

    #[test]
    fn microsoft_types_guid_string_round_trip_contract() {
        let guid = Guid::new(
            0x0102_0304,
            0x0506,
            0x0708,
            [0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10],
        );
        let braced = "{01020304-0506-0708-090a-0b0c0d0e0f10}";
        let plain = "01020304-0506-0708-090a-0b0c0d0e0f10";
        assert_eq!(guid.to_braced_string(), braced);
        assert_eq!(guid.to_plain_string(), plain);
        assert_eq!(Guid::parse(braced), Ok(guid));
        assert_eq!(Guid::parse(plain), Ok(guid));
    }

    #[test]
    fn microsoft_types_v5_uuid_u8_string_matches_source_contract() {
        let expected = Guid::new(
            0x8b9d_4336,
            0x0c82,
            0x54c4,
            [0xb3, 0x15, 0xf1, 0xd2, 0xd2, 0x7e, 0xc6, 0xda],
        );
        assert_eq!(expected, create_v5_uuid(TEST_NAMESPACE_GUID, b"testing"));
    }

    #[test]
    fn microsoft_types_v5_uuid_u16_string_matches_source_contract() {
        let name: Vec<u8> = "testing"
            .encode_utf16()
            .flat_map(u16::to_le_bytes)
            .collect();
        let expected = Guid::new(
            0xe04f_b1f7,
            0x739d,
            0x5d63,
            [0xbb, 0x18, 0xe0, 0xea, 0x00, 0xb1, 0x9e, 0xe8],
        );
        assert_eq!(expected, create_v5_uuid(TEST_NAMESPACE_GUID, &name));
    }
}
