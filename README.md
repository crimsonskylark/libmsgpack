## libmsgpack

This is a library I yanked from an older project and decided to release in case it's helpful to someone.

Right now, it depends on a single MSVC header (`intrin.h`) for the declarations of `_byteswap_ushort`, `_byteswap_ulong` and `_byteswap_uint64` since I had a surprisingly hard time getting MSVC
to compile the byte swap into a single `bswap` instruction otherwise. Everything else is handled internally and the library is completely OS agnostic.

## Functionality

Supports automatic encoding/decoding of most non-variable extension MessagePack types, except `Float32/64` simply because the original project had no need for it.

All fixed sized types are decoded from the byte stream by calling `MessagePack.decode_single( )` and inspecting the returned `MPDecodeResult` value.

## Usage

```cpp
#include <assert.h>
#include <mp.hpp>

int main() {
    constexpr auto buffer_size = 0x1000lu;
    const auto buffer = new mp::mp_u8[ buffer_size ];

[1] mp::MessagePack mpack { };

[2] mpack.initialize_streams(
        0,
        buffer_size,
        buffer,
        reinterpret_cast< stream::MemoryReader >( &memcpy ),
        reinterpret_cast< stream::MemoryWriter >( &memcpy )
    );

[3] mpack.write_u8( 0xa );
[4] mpack.write_uint( 0xa );

[5] mp::MPDecodeResult dr{ mpack.decode_single (  ) };

[5] assert( dr.marker == mp::MPMarker::Uint8 ); // true, because the first write was not opportunistic.

[6] dr = mpack.decode_single( );

[6] assert( dr.marker == mp::MPMarker::PosFixInt ); // true, because the second write was opportunistic; it chose the smallest representation.
}
```

1. Create a new `MessagePack` object.
2. Initialize the internal streams (read/write) with a `buffer` of `buffer_size` bytes. Also pass the memory reading/writing functions -- note both streams will utilize the same buffer. This is required.
3. Write a 1 byte unsigned integer to the stream. The total number of bytes written here is two: one for the marker and the other for the value itself.
4. Write a 1 byte unsigned integer to the stream again, however, this time the encoder will pick the smallest representation which is a `PosFixInt` in this case, resulting in a single byte written.
5. Decode the result, ensure we get the expected marker back.
6. Decode another item, check again we got what we expect.

## Tests

This library is missing a more thorough test suite besides the simple unit tests at `tests.cpp` as well as fuzzing. I plan on adding it eventually, but PRs are welcome.

## Notes

Writing dynamically sized types to the stream is always opportunistic, but writing 