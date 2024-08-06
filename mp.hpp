#pragma once

#ifdef _MSC_VER
#include <intrin.h>
#define bswap_intrin16( v ) ( _byteswap_ushort( v ) )
#define bswap_intrin32( v ) ( _byteswap_ulong( v ) )
#define bswap_intrin64( v ) ( _byteswap_uint64( v ) )
#pragma intrinsic(memset)
#define mmset( ptr, value, size ) ( memset( ( ptr ), ( value ), ( size ) ) )
#elif defined(__GNUC__) || defined(__clang__)
#define bswap_intrin16( v ) ( __builtin_bswap16( v ) )
#define bswap_intrin32( v ) ( __builtin_bswap32( v ) )
#define bswap_intrin64( v ) ( __builtin_bswap64( v ) )
#define mmset( ptr, value, size ) ( memset( ( ptr ), ( value ), ( size ) ) )
#endif

namespace mp
{
    using mp_u32 = unsigned int;
    using mp_u64 = unsigned long long;

    using mp_i32 = signed int;
    using mp_i64 = signed long long;

    using mp_u16 = unsigned short;
    using mp_i16 = signed short;

    using mp_u8 = unsigned char;
    using mp_i8 = signed char;

    static_assert(
        sizeof( mp_u32 ) == 4,
        "incorrectly sized `int` type."
    );

    static_assert(
        sizeof( mp_u64 ) == 8,
        "incorrectly sized `unsigned long long` type."
    );

    static_assert(
        sizeof( mp_u16 ) == 2,
        "incorrectly sized `unsigned short` type."
    );

    static_assert(
        sizeof( mp_u8 ) == 1,
        "incorrectly sized `unsigned long long` type."
    );
}

/*
 * Note: Neither Stream interface takes ownership of the underlying memory.
 * It *always* the caller's responsibility to clean up.
 *
 * Usage:
 *  1) Allocate buffer;
 *  2) Give to Stream(Reader|Writer) as argument, along the buffer size;
 *  3) Read/write data;
 *  4) When no longer required:
 *      4.1) Request buffer via `Stream.start( )`
 *      4.2) Reset internal state via `Stream.reset( )`
 *      4.3) Release buffer
 *      4.4) Stream is now unusable
 *
 *  Notes:
 *      1) Not thread safe by default. User must provide their own locking mechanisms;
 *      2) Overflows are handled silently and internally, no exceptions or invalid memory access are reported to the user.
 */

namespace stream
{
    using MemoryReader = void( * )( void *dst, void *src, mp::mp_u64 size );
    using MemoryWriter = MemoryReader;

    struct Stream
    {
        /**
         * @brief Current cursor position.
         * @return mp::mp_u32
         */
        mp::mp_u32 position( ) const
        {
            return position_;
        }

        /**
         * @brief Total size of the stream. Does not take into account the current cursor position.
         * @return mp::mp_u32
         */
        mp::mp_u32 stream_size( ) const
        {
            return stream_size_;
        }

        /**
         * @brief End address of the buffer managed by this stream.
         * @return mp::mp_u8 *
         */
        mp::mp_u8 *end( ) const
        {
            return buffer_ + stream_size_;
        }

        /**
         * @brief Start address of the buffer managed by this stream.
         * @return mp::mp_u8 *
         */
        mp::mp_u8 *start( ) const
        {
            return buffer_;
        }

    protected:
        mp::mp_u32 position_ { 0 };
        mp::mp_u32 stream_size_ { 0 };
        mp::mp_u8 *buffer_ { nullptr };
    };

    struct StreamReader : Stream
    {
    private:
        MemoryReader reader_ { nullptr };

    public:
        explicit StreamReader(
            const mp::mp_u32 position = 0,
            const mp::mp_u32 stream_size = 0,
            mp::mp_u8 *buffer = nullptr,
            const MemoryReader reader = nullptr
        )
        {
            position_ = position;
            stream_size_ = stream_size;
            buffer_ = buffer;
            reader_ = reader;
        }

        explicit operator bool( ) const
        {
            return reader_ != nullptr && buffer_ != nullptr;
        }

        /* Disallow copies. */
        StreamReader( StreamReader &other ) = delete;
        StreamReader &operator=( StreamReader &other ) = delete;

        /**
         * @brief Reset the internal stream state for this object.
         */
        void reset( )
        {
            position_ = 0;
            stream_size_ = 0;
            buffer_ = nullptr;
            reader_ = nullptr;
        }

        /**
         * @brief Reset the temporal state for this object. Use this when reusing the stream
         * without losing the previously registered `reader` function.
         */
        void reset_temporal( )
        {
            position_ = 0;
            stream_size_ = 0;
            buffer_ = nullptr;
        }

        /**
         * @brief Reset the stream cursor back to the start
         */
        void reset_cursor( )
        {
            position_ = 0;
        }

        /**
         * @brief Set the internal fields of the `StreamWriter` object. Required calling if object was created with default initialization.
         * @param position Position to set the stream at
         * @param stream_size Total size, in bytes, of `buffer`
         * @param buffer Pointer of at least `stream_size` bytes to be managed by `StreamWriter`
         * @param reader Function pointer of type `void ( * ) ( void *src, void *dst, size_t size )` to be used for writing memory
         * @return StreamReader&
         */
        StreamReader &set(
            const mp::mp_u32 position = 0,
            const mp::mp_u32 stream_size = 0,
            mp::mp_u8 *buffer = nullptr,
            const MemoryReader reader = nullptr
        )
        {
            reset ( );

            position_ = position;
            stream_size_ = stream_size;
            buffer_ = buffer;
            reader_ = reader;

            return *this;
        }

        /**
         * @brief Set the internal fields of the `StreamReader` object. Required calling if object was created with default initialization.
         * @param position Position to set the stream at
         * @param stream_size Total size, in bytes, of `buffer`
         * @param buffer Pointer of at least `stream_size` bytes to be managed by `StreamReader`
         * @return StreamReader&
        */
        StreamReader &set_temporal(
            const mp::mp_u32 position = 0,
            const mp::mp_u32 stream_size = 0,
            mp::mp_u8 *buffer = nullptr
        )
        {
            reset_temporal ( );

            position_ = position;
            stream_size_ = stream_size;
            buffer_ = buffer;

            return *this;
        }

    private:
        /**
         * @brief The core of the `StreamReader` interface. Copies `count` bytes from the stream using the internal reader function and writes into `dst`.
         * @remarks Define `_UNSAFE` to remove NULL and overflow checks.
         * @param count Number of bytes in `src` to read from the stream.
         * @param dst Buffer of at least `count` bytes to copy into.
         * @param peek Read without advancing the cursor
         */
        void _read_and_advance( const mp::mp_u32 count, mp::mp_u8 *dst, const bool peek = false )
        {
            const auto read_pos = buffer_ + position_;

#ifndef _UNSAFE
            if ( !count || !dst || count >= stream_size_ || read_pos + count > end ( ) || !*this )
            {
                return;
            }
#endif

            reader_( dst, read_pos, count );

            if ( !peek )
                position_ += count;
        }

        /**
         * @brief Write a plain old data (POD) value to the byte stream and advance the internal cursor.
         * @tparam Ty Plain old data (POD) type to instantiate this parameter with. Valid types are: `mp_i8`, `mp_u8`, `mp_i16`, `mp_u16`, `mp_i32`, `mp_u32`, `mp_i64` and `mp_u64`
         * @param peek Read the data without advancing the cursor
         * @return Ty
         */
        template < typename Ty >
        Ty _read_pod( const bool peek = false )
        {
            Ty pod { };

            _read_and_advance(
                sizeof( Ty ),
                reinterpret_cast< mp::mp_u8* >( &pod ),
                peek
            );

            return pod;
        }

    public:
        /**
         * @brief Read an unsigned 4 byte value from the stream and advance the cursor by 4 bytes.
         * @return mp::mp_u32
         */
        mp::mp_u32 read_u32( )
        {
            return _read_pod< mp::mp_u32 > ( );
        }

        /**
         * @brief Read an unsigned 8 byte value from the stream and advance the cursor by 8 bytes.
         * @return mp::mp_u64
         */
        mp::mp_u64 read_u64( )
        {
            return _read_pod< mp::mp_u64 > ( );
        }

        /**
         * @brief Read a signed 4 byte value from the stream and advance the cursor by 4 bytes.
         * @return mp::mp_i32
         */
        mp::mp_i32 read_i32( )
        {
            return _read_pod< mp::mp_i32 > ( );
        }

        /**
         * @brief Read a signed 8 byte value from the stream and advance the cursor by 8 bytes.
         * @return mp::mp_i64
         */
        mp::mp_i64 read_i64( )
        {
            return _read_pod< mp::mp_i64 > ( );
        }

        /**
         * @brief Read an unsigned 2 byte value from the stream and advance the cursor by 2 bytes.
         * @return mp::mp_u16
         */
        mp::mp_u16 read_u16( )
        {
            return _read_pod< mp::mp_u16 > ( );
        }

        /**
         * @brief Read a signed 2 byte value from the stream and advance the cursor by 2 bytes.
         * @return mp::mp_i16
         */
        mp::mp_i16 read_i16( )
        {
            return _read_pod< mp::mp_i16 > ( );
        }

        /**
         * @brief Read an unsigned byte from the stream and advance the cursor by 1 byte.
         * @return mp::mp_u8
         */
        mp::mp_u8 read_u8( )
        {
            return _read_pod< mp::mp_u8 > ( );
        }

        /**
         * @brief Read a signed byte from the stream and advance the cursor by 1 byte.
         * @return mp::mp_i8
         */
        mp::mp_i8 read_i8( )
        {
            return _read_pod< mp::mp_i8 > ( );
        }

        /**
         * @brief Read an unsigned 4 byte value from the stream without advancing the cursor.
         * @return mp::mp_u32
         */
        mp::mp_u32 peek_u32( )
        {
            return _read_pod< mp::mp_u32 >( true );
        }

        /**
         * @brief Read an unsigned 8 byte value from the stream without advancing the cursor.
         * @return mp::mp_u64
         */
        mp::mp_u64 peek_u64( )
        {
            return _read_pod< mp::mp_u64 >( true );
        }

        /**
         * @brief Read a signed 4 byte value from the stream without advancing the cursor.
         * @return mp::mp_i32
         */
        mp::mp_i32 peek_i32( )
        {
            return _read_pod< mp::mp_i32 >( true );
        }

        /**
         * @brief Read a signed 8 byte value from the stream without advancing the cursor.
         * @return mp::mp_i64
         */
        mp::mp_i64 peek_i64( )
        {
            return _read_pod< mp::mp_i64 >( true );
        }

        /**
         * @brief Read an unsigned two byte value from the stream without advancing the cursor.
         * @return mp::mp_u16
         */
        mp::mp_u16 peek_u16( )
        {
            return _read_pod< mp::mp_u16 >( true );
        }

        /**
         * @brief Read a signed two byte value from the stream without advancing the cursor.
         * @return mp::mp_i16
         */
        mp::mp_i16 peek_i16( )
        {
            return _read_pod< mp::mp_i16 >( true );
        }

        /**
         * @brief Read an unsigned byte from the stream without advancing the cursor.
         * @return mp::mp_u8
         */
        mp::mp_u8 peek_u8( )
        {
            return _read_pod< mp::mp_u8 >( true );
        }

        /**
         * @brief Read a signed byte from the stream without advancing the cursor.
         * @return mp::mp_i8
         */
        mp::mp_i8 peek_i8( )
        {
            return _read_pod< mp::mp_i8 >( true );
        }

        /**
         * @brief Read at most `count` bytes from the stream and advance the internal cursor by the same amount.
         * @param count Size, in bytes, of the data pointed to by `src`.
         * @param dst Buffer containing at least `count` bytes.
         * @return StreamReader&
         */
        StreamReader &read( const mp::mp_u32 count, mp::mp_u8 *dst )
        {
            _read_and_advance( count, dst );

            return *this;
        }
    };

    struct StreamWriter : Stream
    {
    private:
        MemoryWriter writer_ { nullptr };

    public:
        explicit StreamWriter(
            const mp::mp_u32 position = 0,
            const mp::mp_u32 stream_size = 0,
            mp::mp_u8 *buffer = nullptr,
            const MemoryWriter writer = nullptr
        )
        {
            position_ = position;
            stream_size_ = stream_size;
            buffer_ = buffer;
            writer_ = writer;
        }

        explicit operator bool( ) const
        {
            return writer_ != nullptr && buffer_ != nullptr;
        }

        /* Disallow copies. */
        StreamWriter( StreamWriter &other ) = delete;
        StreamWriter &operator=( StreamWriter &other ) = delete;

        /**
         * @brief Reset the internal stream state for this object.
         */
        void reset( )
        {
            position_ = 0;
            stream_size_ = 0;
            buffer_ = nullptr;
            writer_ = nullptr;
        }

        /**
        * @brief Reset the temporal state for this object. Use this when reusing the stream
        * without losing the previously registered `writer` function.
        */
        void reset_temporal( )
        {
            position_ = 0;
            stream_size_ = 0;
            buffer_ = nullptr;
        }

        /**
         * @brief Reset the stream cursor back to the start
         */
        void reset_cursor( )
        {
            position_ = 0;
        }

        /*
         * @brief Clear the stream and reset the cursor
         */
        void clear( )
        {
            mmset( buffer_, 0, stream_size_ );
            reset_cursor ( );
        }

        /**
         * @brief Set the internal fields of the `StreamWriter` object. Required calling if object was created with default initialization.
         * @param position Position to set the stream at
         * @param stream_size Total size, in bytes, of `buffer`
         * @param buffer Pointer of at least `stream_size` bytes to be managed by `StreamWriter`
         * @param writer Function pointer of type `void ( * ) ( void *src, void *dst, size_t size )` to be used for writing memory
         * @return StreamWriter&
         */
        StreamWriter &set(
            const mp::mp_u32 position = 0,
            const mp::mp_u32 stream_size = 0,
            mp::mp_u8 *buffer = nullptr,
            const MemoryWriter writer = nullptr
        )
        {
            reset ( );

            position_ = position;
            stream_size_ = stream_size;
            buffer_ = buffer;
            writer_ = writer;

            return *this;
        }

        /**
         * @brief Set the internal fields of the `StreamWriter` object.
         * @param position Position to set the stream at
         * @param stream_size Total size, in bytes, of `buffer`
         * @param buffer Pointer of at least `stream_size` bytes to be managed by `StreamWriter`
         * @return StreamReader&
        */
        StreamWriter &set_temporal(
            const mp::mp_u32 position = 0,
            const mp::mp_u32 stream_size = 0,
            mp::mp_u8 *buffer = nullptr
        )
        {
            reset_temporal ( );

            position_ = position;
            stream_size_ = stream_size;
            buffer_ = buffer;

            return *this;
        }

        /**
         * @brief Write a plain old data (POD) value to the byte stream and advance the internal cursor.
         * @tparam Ty Plain old data (POD) type to instantiate this parameter with. Valid types are: `mp_i8`, `mp_u8`, `mp_i16`, `mp_u16`, `mp_i32`, `mp_u32`, `mp_i64` and `mp_u64`
         * @param value Value to write to the stream.
         */
        template < typename Ty >
        void write_pod( Ty value )
        {
            Ty pod { value };

            _write_and_advance(
                sizeof( Ty ),
                reinterpret_cast< mp::mp_u8* >( &pod )
            );
        }

    private:
        /**
         * @brief The core of the `StreamWriter` interface. Copies `count` bytes from `src` using the internal writer function and writes into the byte stream.
         * @remarks Define `_UNSAFE` to remove NULL and overflow checks.
         * @param count Number of bytes in `src` to write into the stream.
         * @param src Buffer of at least `count` bytes to copy from.
         */
        void _write_and_advance( const mp::mp_u32 count, mp::mp_u8 *src )
        {
            const auto write_pos = buffer_ + position_;

#ifndef _UNSAFE
            if ( !count || !src || count >= stream_size_ || write_pos + count > end ( ) || !*this )
            {
                return;
            }
#endif

            writer_( write_pos, src, count );

            position_ += count;
        }

    public:
        /**
         * @brief Write a single unsigned 4 byte value to the stream and advance the cursor by 4 if the stream has not reached its end.
         * @param value Unsigned 4 byte value to write to the stream
         * @return StreamWriter&
         */
        StreamWriter &write_u32( const mp::mp_u32 value )
        {
            write_pod< mp::mp_u32 >( value );
            return *this;
        }

        /**
         * @brief Write a single unsigned 8 byte value to the stream and advance the cursor by 8 if the stream has not reached its end.
         * @param value Unsigned 8 byte value to write to the stream
         * @return StreamWriter&
         */
        StreamWriter &write_u64( const mp::mp_u64 value )
        {
            write_pod< mp::mp_u64 >( value );
            return *this;
        }

        /**
         * @brief Write a single signed 4 byte value to the stream and advance the cursor by 4 if the stream has not reached its end.
         * The value will be cast to its unsigned counterpart before the endianess change.
         * @param value Signed 4 byte value to write to the stream
         * @return StreamWriter&
         */
        StreamWriter &write_i32( const mp::mp_i32 value )
        {
            write_pod< mp::mp_i32 >( value );
            return *this;
        }

        /**
         * @brief Write a single unsigned 8 byte value to the stream and advance the cursor by 8 if the stream has not reached its end.
         * The value will be cast to its unsigned counterpart before the endianess change.
         * @param value Signed 8 byte value to write to the stream
         * @return StreamWriter&
         */
        StreamWriter &write_i64( const mp::mp_i64 value )
        {
            write_pod< mp::mp_i64 >( value );
            return *this;
        }

        /**
         * @brief Write a single unsigned 2 byte value to the stream and advance the cursor by 2 if the stream has not reached its end.
         * @param value Unsigned 2 byte value to write to the stream
         * @return StreamWriter&
         */
        StreamWriter &write_u16( const mp::mp_u16 value )
        {
            write_pod< mp::mp_u16 >( value );
            return *this;
        }

        /**
         * @brief Write a single signed 2 byte value to the stream and advance the cursor by 2 if the stream has not reached its end.
         * The value will be cast to its unsigned counterpart before the endianess change.
         * @param value Signed 2 byte value to write to the stream
         * @return StreamWriter&
         */
        StreamWriter &write_i16( const mp::mp_i16 value )
        {
            write_pod< mp::mp_i16 >( value );
            return *this;
        }

        /**
        * @brief Write a single unsigned byte to the stream and advance the cursor by 1 if the stream has not reached its end.
        * @param value Unsigned byte to write to the stream
        * @return StreamWriter&
        */
        StreamWriter &write_u8( const mp::mp_u8 value )
        {
            write_pod< mp::mp_u8 >( value );
            return *this;
        }

        /**
         * @brief Write a single signed byte to the stream and advance the cursor by 1 if the stream has not reached its end.
         * @param value Signed byte to write to the stream
         * @return StreamWriter&
         */
        StreamWriter &write_i8( const mp::mp_i8 value )
        {
            write_pod< mp::mp_i8 >( value );
            return *this;
        }

        /**
         * @brief Write at most `count` bytes to the stream and advance the internal cursor by the same amount.
         * @param count Size, in bytes, of the data pointed to by `src`.
         * @param src Buffer containing at least `count` bytes.
         * @return StreamWriter&
         */
        StreamWriter &write( const mp::mp_u32 count, mp::mp_u8 *src )
        {
            _write_and_advance( count, src );
            return *this;
        }
    };
}

namespace limits
{
    /*
     * Taken from the MSVC standard library and converted to constexpr instead of C macros.
     */
    constexpr auto int8_min = ( -127i8 - 1 );
    constexpr auto int16_min = ( -32767i16 - 1 );
    constexpr auto int32_min = ( -2147483647i32 - 1 );
    constexpr auto int64_min = ( -9223372036854775807i64 - 1 );
    constexpr auto int8_max = 127i8;
    constexpr auto int16_max = 32767i16;
    constexpr auto int32_max = 2147483647i32;
    constexpr auto int64_max = 9223372036854775807i64;
    constexpr auto uint8_max = 0xffui8;
    constexpr auto uint16_max = 0xffffui16;
    constexpr auto uint32_max = 0xffffffffui32;
    constexpr auto uint64_max = 0xffffffffffffffffui64;
}

namespace mp
{
    namespace value_limits
    {
        // - (2 ^ 63)
        constexpr auto IntMin = -9223372036854775807i64 - 1;

        // ( 2^64 ) - 1
        constexpr auto IntMax = 18446744073709551615llu;

        //  ( 2^32 ) - 1
        constexpr auto BinMax = 4294967295LU;

        //  ( 2^32 ) - 1
        constexpr auto StringMax = 4294967295LU;

        //  ( 2^32 ) - 1
        constexpr auto ArrayMax = 4294967295LU;

        //  ( 2^32 ) - 1
        constexpr auto KVMax = 4294967295LU;

        constexpr auto FixArrayMax = 15LU;
        constexpr auto Array16Max = 65535LU;
        constexpr auto Array32Max = 4294967295LU;

        constexpr auto FixMapMax = 15LU;
        constexpr auto Map16Max = 65535LU;
        constexpr auto Map32Max = 4294967295LU;

        constexpr auto PosFixIntMax = 127LU;
    }

    namespace type
    {
        enum class TypeValue : mp_u32
        {
            Integer,
            Nil,
            Boolean,
            Float, /* unsupported */
            Raw,
            String,
            Binary,
            Array,
            Map,
            Extension
        };

        struct MPInteger
        {
            static mp_u32 value( ) { return static_cast< mp_u32 >( TypeValue::Integer ); }
        };

        struct MPNil
        {
            static mp_u32 value( ) { return static_cast< mp_u32 >( TypeValue::Nil ); }
        };

        struct MPBoolean
        {
            static mp_u32 value( ) { return static_cast< mp_u32 >( TypeValue::Boolean ); }
        };

        struct MPRaw
        {
            static mp_u32 value( ) { return static_cast< mp_u32 >( TypeValue::Raw ); }
        };

        struct MPString
        {
            static mp_u32 value( ) { return static_cast< mp_u32 >( TypeValue::String ); }
        };

        struct MPBinary
        {
            static mp_u32 value( ) { return static_cast< mp_u32 >( TypeValue::Binary ); }
        };

        struct MPArray
        {
            static mp_u32 value( ) { return static_cast< mp_u32 >( TypeValue::Array ); }
        };

        struct MPMap
        {
            static mp_u32 value( ) { return static_cast< mp_u32 >( TypeValue::Map ); }
        };
    }

    enum class MPMarker : mp::mp_u8
    {
        PosFixInt,
        FixMap    = 0x80,
        FixArray  = 0x90,
        FixStr    = 0xa0,
        Nil       = 0xc0,
        Unused    = 0xc1,
        False     = 0xc2,
        True      = 0xc3,
        Bin8      = 0xc4,
        Bin16     = 0xc5,
        Bin32     = 0xc6,
        Ext8      = 0xc7,
        Ext16     = 0xc8,
        Ext32     = 0xc9,
        Float32   = 0xca,
        Float64   = 0xcb,
        Uint8     = 0xcc,
        Uint16    = 0xcd,
        Uint32    = 0xce,
        Uint64    = 0xcf,
        Int8      = 0xd0,
        Int16     = 0xd1,
        Int32     = 0xd2,
        Int64     = 0xd3,
        FixExt1   = 0xd4,
        FixExt2   = 0xd5,
        FixExt4   = 0xd6,
        FixExt8   = 0xd7,
        FixExt16  = 0xd8,
        Str8      = 0xd9,
        Str16     = 0xda,
        Str32     = 0xdb,
        Array16   = 0xdc,
        Array32   = 0xdd,
        Map16     = 0xde,
        Map32     = 0xdf,
        NegFixInt = 0xe0
    };

    struct MPFixExt1
    {
        mp_u8 type;
        mp_u8 data;
    };

    struct MPFixExt2
    {
        mp_u8 type;
        mp_u8 data[ 2 ];
    };

    struct MPFixExt4
    {
        mp_u8 type;
        mp_u8 data[ 4 ];
    };

    struct MPFixExt8
    {
        mp_u8 type;
        mp_u8 data[ 8 ];
    };

    struct MPFixExt16
    {
        mp_u8 type;
        mp_u8 data[ 16 ];
    };

    struct MPDecodeResult
    {
        MPMarker marker; // Marker of the latest value decoded
        mp_u32 size;     // Size of the value, in bytes. Mostly relevant for dynamically sized types.

        union
        {
            bool as_bool;

            mp_u8 as_u8;
            mp_i8 as_i8;

            mp_i16 as_i16;
            mp_u16 as_u16;

            mp_i32 as_i32;
            mp_u32 as_u32;

            mp_i64 as_i64;
            mp_u64 as_u64;

            MPFixExt1 as_fixext1;
            MPFixExt2 as_fixext2;
            MPFixExt4 as_fixext4;
            MPFixExt8 as_fixext8;
            MPFixExt16 as_fixext16;

            mp_u8 as_fixstr[ 31 ];
        } result; // Holds the result for statically sized types such as FixInt, NegFixInt, Uint8/16/32/64, Int8/16/32/64, FixExt1/2/4/8/16

        explicit operator bool( ) const { return marker != MPMarker::Unused; }

        /**
         * @brief Check if this result's marker denotes the start of a fixext type.
         * @return bool
         */
        bool is_fixext( ) const
        {
            return (
                marker == MPMarker::FixExt1 ||
                marker == MPMarker::FixExt2 ||
                marker == MPMarker::FixExt4 ||
                marker == MPMarker::FixExt8 ||
                marker == MPMarker::FixExt16
            );
        }

        /**
         * @brief Check if this result's marker denotes the start of an array.
         * @return bool
         */
        bool is_array( ) const
        {
            return (
                marker == MPMarker::FixArray ||
                marker == MPMarker::Array16 ||
                marker == MPMarker::Array32
            );
        }

        /**
         * @brief Check if this result's marker denotes any integer type.
         * @return bool
         */
        bool is_integer( ) const
        {
            return (
                marker == MPMarker::PosFixInt ||
                marker == MPMarker::NegFixInt ||
                marker == MPMarker::Uint8 ||
                marker == MPMarker::Int8 ||
                marker == MPMarker::Uint16 ||
                marker == MPMarker::Int16 ||
                marker == MPMarker::Uint32 ||
                marker == MPMarker::Int32 ||
                marker == MPMarker::Uint64 ||
                marker == MPMarker::Int64
            );
        }


        /**
         * @brief Check if this result's marker denotes any string type.
         * @return bool
         */
        bool is_str( ) const
        {
            return (
                marker == MPMarker::FixStr ||
                marker == MPMarker::Str8 ||
                marker == MPMarker::Str16 ||
                marker == MPMarker::Str32
            );
        }

        /**
         * @brief Check if this result's marker denotes the start of any binary type.
         * @return bool
         */
        bool is_bin( ) const
        {
            return (
                marker == MPMarker::Bin8 ||
                marker == MPMarker::Bin16 ||
                marker == MPMarker::Bin32
            );
        }

        /**
         * @brief Check if this result's marker denotes the start of any non-fixed extension type.
         * @return bool
         */
        bool is_ext( ) const
        {
            return (
                marker == MPMarker::Ext8 ||
                marker == MPMarker::Ext16 ||
                marker == MPMarker::Ext32
            );
        }

        /**
         * @brief Check if this result's marker denotes the start of any boolean type.
         * @return bool
         */
        bool is_bool( ) const
        {
            return (
                marker == MPMarker::True ||
                marker == MPMarker::False
            );
        }

        /**
         * @brief Check if this result's marker denotes a `Nil` value
         * @return bool
         */
        bool is_nil( ) const
        {
            return marker == MPMarker::Nil;
        }
    };

    struct MessagePack
    {
    private:
        /**
         * @brief Internal object used for reading from the byte stream.
         */
        stream::StreamReader sr_ { };

        /**
         * @brief Internal object used for writing to the byte stream.
         */
        stream::StreamWriter wr_ { };

        /**
         * @brief Pointer to the user allocated buffer used by `StreamReader` and `StreamReader`. Call `reset_all( )` before releasing this memory.
         */
        mp::mp_u8 *buffer_ { nullptr };

        /**
         * @brief Local copy of the memory reader function. Used in decoding operations for copying larger types from the stream.
         */
        stream::MemoryReader reader_ { nullptr };

    public:
        /**
         * @brief Initialize the `StreamReader` and `StreamWriter` object with `buffer` of size `stream_size` at cursor `position`.
         * @param position Position in the stream buffer to start operations at
         * @param stream_size Total size, in bytes, of the stream buffer
         * @param buffer Pointer to the underlying stream buffer
         * @param reader Function pointer of type `void ( * ) ( void *src, void *dst, size_t size )` to be used for reading memory
         * @param writer Function pointer of type `void ( * ) ( void *src, void *dst, size_t size )` to be used for writing memory
         */
        void initialize_streams( const mp::mp_u32 position = 0,
                                 const mp::mp_u32 stream_size = 0,
                                 mp::mp_u8 *buffer = nullptr,
                                 const stream::MemoryReader reader = nullptr,
                                 const stream::MemoryWriter writer = nullptr
        )
        {
            sr_.set( position, stream_size, buffer, reader );
            wr_.set( position, stream_size, buffer, writer );

            buffer_ = buffer;
            reader_ = reader;
        }

        /**
         * @brief Retrieve a pointer to the underlying buffer.
         * @return mp::mp_u8
         */
        mp::mp_u8 *stream_buffer( ) const
        {
            return buffer_;
        }

        /**
         * @brief Return the previously set stream size
         * @return The size, in bytes, of the underlying buffer
         */
        mp::mp_u32 stream_size( ) const
        {
            return sr_.stream_size ( );
        }

        /**
         * @brief Position of the read cursor in the stream.
         * @return mp::mp_up32
         */
        mp::mp_u32 read_cursor( ) const
        {
            return sr_.position ( );
        }

        /**
         * @brief Position of the write cursor in the stream.
         * @return mp::mp_u32
         */
        mp::mp_u32 write_cursor( ) const
        {
            return wr_.position ( );
        }

        /**
         * @brief Reset both streams and zero the stream. Required calling before releasing the underlying memory.
         */
        void reset_all( )
        {
            wr_.clear ( );
            sr_.reset ( );
            wr_.reset ( );
        }

        /**
         * @brief Reset the temporal state for this object. Use this when reusing the stream object
         * without losing the previously registered memory readers function.
         */
        void reset_temporal( )
        {
            wr_.clear ( );

            sr_.reset_temporal ( );
            wr_.reset_temporal ( );
            buffer_ = nullptr;
        }

        /**
         * @brief Reset cursors for both streams managed by this object.
         */
        void reset_cursors( )
        {
            sr_.reset_cursor ( );
            wr_.reset_cursor ( );
        }

        /**
         * @brief Reset both stream cursors and zero the underlying buffer.
         */
        void reset_and_clear( )
        {
            wr_.clear ( );
            reset_cursors ( );
        }

        /**
         * @brief
         * @param marker Value of type `MPMarker` denoting the start of a MessagePack value.
         */
        void write_marker( MPMarker marker )
        {
            wr_.write_u8( static_cast< mp::mp_u8 >( marker ) );
        }

        /**
         * @brief Peek a byte from the stream and attempt to decode it as MessagePack type.
         * @return An `MPMarker` value if the peeked byte corresponds to a valid MessagePack type or `MPMarker::Unused`
         */
        MPMarker peek_marker( )
        {
            const auto byte = sr_.peek_u8 ( );

            switch ( static_cast< MPMarker >( byte ) )
            {
            case MPMarker::PosFixInt:
            case MPMarker::FixMap:
            case MPMarker::FixArray:
            case MPMarker::FixStr:
            case MPMarker::Nil:
            case MPMarker::False:
            case MPMarker::True:
            case MPMarker::Bin8:
            case MPMarker::Bin16:
            case MPMarker::Bin32:
            case MPMarker::Ext8:
            case MPMarker::Ext16:
            case MPMarker::Ext32:
            case MPMarker::Float32:
            case MPMarker::Float64:
            case MPMarker::Uint8:
            case MPMarker::Uint16:
            case MPMarker::Uint32:
            case MPMarker::Uint64:
            case MPMarker::Int8:
            case MPMarker::Int16:
            case MPMarker::Int32:
            case MPMarker::Int64:
            case MPMarker::FixExt1:
            case MPMarker::FixExt2:
            case MPMarker::FixExt4:
            case MPMarker::FixExt8:
            case MPMarker::FixExt16:
            case MPMarker::Str8:
            case MPMarker::Str16:
            case MPMarker::Str32:
            case MPMarker::Array16:
            case MPMarker::Array32:
            case MPMarker::Map16:
            case MPMarker::Map32:
            case MPMarker::NegFixInt:
                return static_cast< MPMarker >( byte );
            case MPMarker::Unused:
                return MPMarker::Unused;
            }

            return MPMarker::Unused;
        }

        MessagePack &write_negfixint( const mp_i8 value )
        {
            wr_.write_u8(
                0xe0 | static_cast< mp_u8 >( value & 0x1F )
            );

            return *this;
        }

        /**
         * @brief Handles the writing of a [type marker] [value] to the byte stream
         * @note This function is opportunistic and will always opt for the smallest data type, in some cases overwriting user choice. This behaviour cannot be overriden.
         * @remark For arrays, including `FixArray`, call `array( )` first to denote the start of an array.
         * @remark For maps, including `FixMap`, call `map( )` first to denote the start of the map.
         * @param data Treated as a value or a pointer to a value depending on `kind`
         * @param size Size of `data` or the buffer pointed to by `data`
         * @param kind Marker describing the type in the MessagePack type system
         * @return MessagePack&
         */
        MessagePack &write_raw_value( const mp_u64 data, const mp_u64 size, const MPMarker kind )
        {
            /*
             * Start with all the edge cases.
             */

            if ( kind == MPMarker::NegFixInt )
            {
                wr_.write_u8(
                    0xe0 | static_cast< mp_u8 >( static_cast< mp_i8 >( data & 0x1F ) )
                );

                return *this;
            }

            if ( kind == MPMarker::PosFixInt )
            {
                wr_.write_u8( data & 0x7f );

                return *this;
            }

            if ( kind == MPMarker::FixStr )
            {
                const auto length = static_cast< mp::mp_u8 >( size & 0x1f );

                wr_.write_u8( static_cast< mp::mp_u8 >( kind ) | ( length ) )
                   .write( length, reinterpret_cast< mp::mp_u8* >( data ) );

                return *this;
            }

            if (
                kind == MPMarker::Nil ||
                kind == MPMarker::False ||
                kind == MPMarker::True
            )
            {
                /*
                 * The value of these data types is their marker.
                 */
                write_marker( kind );
                return *this;
            }

            if (
                kind == MPMarker::Bin8 ||
                kind == MPMarker::Bin16 ||
                kind == MPMarker::Bin32
            )
            {
                mp_u32 length = static_cast< mp_u32 >( size );

                if ( size <= limits::uint8_max )
                {
                    length &= 0xff;
                    write_marker( MPMarker::Bin8 );
                    wr_.write_u8( static_cast< mp::mp_u8 >( length ) );
                }
                else if ( size <= limits::uint16_max )
                {
                    length &= 0xffff;
                    write_marker( MPMarker::Bin16 );
                    wr_.write_u16( bswap_intrin16( static_cast<mp::mp_u16>(length) ) );
                }
                else if ( size <= limits::uint32_max )
                {
                    length &= 0xffffffff;
                    write_marker( MPMarker::Bin32 );
                    wr_.write_u32( bswap_intrin32( length ) );
                }

                wr_.write(
                    length,
                    reinterpret_cast< mp::mp_u8* >( data )
                );

                return *this;
            }

            if (
                kind == MPMarker::Str8 ||
                kind == MPMarker::Str16 ||
                kind == MPMarker::Str32
            )
            {
                mp_u32 length = static_cast< mp_u32 >( size );

                if ( size <= limits::uint8_max )
                {
                    length &= 0xff;
                    write_marker( MPMarker::Str8 );
                    wr_.write_u8( static_cast< mp::mp_u8 >( length ) );
                }
                else if ( size <= limits::uint16_max )
                {
                    length &= 0xffff;
                    write_marker( MPMarker::Str16 );
                    wr_.write_u16( bswap_intrin16( static_cast<mp::mp_u16>(length) ) );
                }
                else if ( size <= limits::uint32_max )
                {
                    length &= 0xffffffff;
                    write_marker( MPMarker::Str32 );
                    wr_.write_u32( bswap_intrin32( length ) );
                }

                wr_.write(
                    length,
                    reinterpret_cast< mp::mp_u8* >( data )
                );

                return *this;
            }

            write_marker( kind );

            if (
                kind == MPMarker::Uint8 ||
                kind == MPMarker::Int8
            )
            {
                wr_.write_u8(
                    static_cast< mp::mp_u8 >( data & 0xff )
                );

                return *this;
            }

            if (
                kind == MPMarker::Uint16 ||
                kind == MPMarker::Int16
            )
            {
                wr_.write_u16(
                    bswap_intrin16( static_cast<mp::mp_u16>(data & 0xffff) )
                );

                return *this;
            }

            if (
                kind == MPMarker::Uint32 ||
                kind == MPMarker::Int32
            )
            {
                wr_.write_u32(
                    bswap_intrin32( static_cast<mp_u32>(data & 0xffffffff) )
                );

                return *this;
            }

            if (
                kind == MPMarker::Uint64 ||
                kind == MPMarker::Int64
            )
            {
                wr_.write_u64(
                    bswap_intrin64( data )
                );
            }

            if ( kind == MPMarker::FixExt1 )
            {
                /* one byte for the type + 1 for the byte array */
                wr_.write( sizeof( MPFixExt1 ), reinterpret_cast< mp::mp_u8* >( data ) );
            }

            if ( kind == MPMarker::FixExt2 )
            {
                /* one byte for the type + 2 bytes for the byte array */
                wr_.write( sizeof( MPFixExt2 ), reinterpret_cast< mp::mp_u8* >( data ) );
            }

            if ( kind == MPMarker::FixExt4 )
            {
                /* one byte for the type + 4 bytes for the byte array */
                wr_.write( sizeof( MPFixExt4 ), reinterpret_cast< mp::mp_u8* >( data ) );
            }

            if ( kind == MPMarker::FixExt8 )
            {
                /* one byte for the type + 8 bytes for the byte array */
                wr_.write( sizeof( MPFixExt8 ), reinterpret_cast< mp::mp_u8* >( data ) );
            }

            if ( kind == MPMarker::FixExt16 )
            {
                /* one byte for the type + 16 bytes for the byte array */
                wr_.write( sizeof( MPFixExt16 ), reinterpret_cast< mp::mp_u8* >( data ) );
            }

            return *this;
        }

        /**
         * @brief Mark the start of a `Array` object in the byte stream. The chosen array type depends on the `num_elem` parameter.
         * @param num_elem Number of key-value pairs in this map. Both keys and values can be any MessagePack type.
         * @return MessagePack&
         */
        MessagePack &start_array( const mp::mp_u64 num_elem )
        {
            if ( num_elem <= mp::value_limits::FixArrayMax )
            {
                wr_.write_u8(
                    static_cast< mp::mp_u8 >( mp::MPMarker::FixArray ) | static_cast< mp::mp_u8 >( num_elem & 0xf )
                );
            }
            else if ( num_elem <= mp::value_limits::Array16Max )
            {
                write_marker( MPMarker::Array16 );
                wr_.write_u16( bswap_intrin16( static_cast<mp::mp_u16>(num_elem & 0xffff) ) );
            }
            else
            {
                write_marker( MPMarker::Array32 );
                wr_.write_u32( bswap_intrin32( static_cast<mp::mp_u32>(num_elem & 0xffffffff) ) );
            }

            return *this;
        }

        /**
         * @brief Mark the start of a `Map` object in the byte stream. The map type chosen depends on the `num_pairs` parameter.
         * @param num_pairs Number of key-value pairs in this map. Both keys and values can be any MessagePack type.
         * @remark Not every language-specific decoder supports arbitrary key types. Keep this in mind when writing values
         * to the map. Using the recommended Python decoder, for instance, requires you to explicitly allow integer keys.
         * @return MessagePack&
         */
        MessagePack &start_map( const mp::mp_u64 num_pairs )
        {
            if ( num_pairs <= mp::value_limits::Map16Max )
            {
                wr_.write_u8( static_cast< mp::mp_u8 >( mp::MPMarker::FixMap ) | static_cast< mp::mp_u8 >( num_pairs & 0xf ) );
            }
            else if ( num_pairs <= mp::value_limits::Map32Max )
            {
                write_marker( MPMarker::Map16 );
                wr_.write_u16( bswap_intrin16( static_cast<mp::mp_u16>(num_pairs & 0xffff) ) );
            }
            else
            {
                write_marker( MPMarker::Map32 );
                wr_.write_u32( bswap_intrin32( static_cast<mp::mp_u32>(num_pairs & 0xffffffff) ) );
            }

            return *this;
        }

        /**
         * @brief Read an 8 byte unsigned integer from the byte stream and advance the cursor by 8 bytes.
         * @return mp::mp_u64
         */
        mp::mp_u64 read_u64( )
        {
            return bswap_intrin64( sr_.read_u64() );
        }

        /**
         * @brief Read a 4 byte unsigned integer from the byte stream and advance the cursor by 4 bytes.
         * @return mp::mp_u32
         */
        mp::mp_u32 read_u32( )
        {
            return bswap_intrin32( sr_.read_u32() );
        }

        /**
         * @brief Read a 2 byte unsigned integer from the byte stream and advance the cursor by 2 bytes.
         * @return mp::mp_u16
         */
        mp::mp_u16 read_u16( )
        {
            return bswap_intrin16( sr_.read_u16() );
        }

        /**
         * @brief Read an unsigned byte integer from the byte stream and advance the cursor by 1 byte.
         * @return mp::mp_u8
         */
        mp::mp_u8 read_u8( )
        {
            return sr_.read_u8 ( );
        }

        /**
         * @brief Read an 8 byte signed integer from the byte stream and advance the cursor by 8 bytes.
         * @return mp::mp_i64
         */
        mp::mp_i64 read_i64( )
        {
            return static_cast< mp_i64 >(
                bswap_intrin64( static_cast<mp_u64>(sr_.read_i64()) )
            );
        }

        /**
         * @brief Read a 4 byte signed integer from the byte stream and advance the cursor by 4 bytes.
         * @return mp::mp_i32
         */
        mp::mp_i32 read_i32( )
        {
            return static_cast< mp_i32 >(
                bswap_intrin32( static_cast<mp_u32>(sr_.read_i32()) )
            );
        }

        /**
         * @brief Read a 2 byte signed integer from the byte stream and advance the cursor by 2 bytes.
         * @return mp::mp_i16
         */
        mp::mp_i16 read_i16( )
        {
            return bswap_intrin16( sr_.read_i16() );
        }

        /**
         * @brief Read a signed byte integer from the byte stream and advance the cursor by 1 byte.
         * @return mp::mp_i8
         */
        mp::mp_i8 read_i8( )
        {
            return sr_.read_i8 ( );
        }

        /**
         * @brief Write a single unsigned 4 byte value to the stream and advance the cursor by 4 if the stream has not reached its end.
         * @param value Unsigned 4 byte value to write to the stream
         * @return MessagePack&
         */
        MessagePack &write_u32( const mp::mp_u32 value )
        {
            write_raw_value( value, sizeof( mp::mp_u32 ), MPMarker::Uint32 );
            return *this;
        }

        /**
         * @brief Write a single unsigned 8 byte value to the stream and advance the cursor by 8 if the stream has not reached its end.
         * @param value Unsigned 8 byte value to write to the stream
         * @return MessagePack&
         */
        MessagePack &write_u64( const mp::mp_u64 value )
        {
            write_raw_value( value, sizeof( mp::mp_u64 ), MPMarker::Uint64 );
            return *this;
        }

        /**
         * @brief Write a single signed 4 byte value to the stream and advance the cursor by 4 if the stream has not reached its end.
         * The value will be cast to its unsigned counterpart before the endianess change.
         * @param value Signed 4 byte value to write to the stream
         * @return MessagePack&
         */
        MessagePack &write_i32( const mp::mp_i32 value )
        {
            write_raw_value( value, sizeof( mp::mp_i32 ), MPMarker::Int32 );
            return *this;
        }

        /**
         * @brief Write a single unsigned 8 byte value to the stream and advance the cursor by 8 if the stream has not reached its end.
         * The value will be cast to its unsigned counterpart before the endianess change.
         * @param value Signed 8 byte value to write to the stream
         * @return MessagePack&
         */
        MessagePack &write_i64( const mp::mp_i64 value )
        {
            write_raw_value( value, sizeof( mp::mp_i64 ), MPMarker::Int64 );
            return *this;
        }

        /**
         * @brief Write a single unsigned 2 byte value to the stream and advance the cursor by 2 if the stream has not reached its end.
         * @param value Unsigned 2 byte value to write to the stream
         * @return MessagePack&
         */
        MessagePack &write_u16( const mp::mp_u16 value )
        {
            write_raw_value( value, sizeof( mp::mp_u16 ), MPMarker::Uint16 );
            return *this;
        }

        /**
         * @brief Write a single signed 2 byte value to the stream and advance the cursor by 2 if the stream has not reached its end.
         * The value will be cast to its unsigned counterpart before the endianess change.
         * @param value Signed 2 byte value to write to the stream
         * @return MessagePack&
         */
        MessagePack &write_i16( const mp::mp_i16 value )
        {
            write_raw_value( value, sizeof( mp::mp_i16 ), MPMarker::Int16 );
            return *this;
        }

        MessagePack &write_posfixint( const mp::mp_u8 value )
        {
            write_raw_value( value, sizeof( mp::mp_u8 ), MPMarker::PosFixInt );
            return *this;
        }

        MessagePack &write_fixint( const mp::mp_i8 value )
        {
            if ( value > 0 )
                write_posfixint( value );
            else
                write_negfixint( value );

            return *this;
        }

        /**
         * @brief Write an unsigned integer to the stream using the smallest possible representation.
         * @param value Integer value to write to the stream
         * @return MessagePack&
         */
        MessagePack &write_uint( const mp::mp_u64 value )
        {
            if ( value <= value_limits::PosFixIntMax )
                write_posfixint( value & 0x7F );
            else if ( value <= limits::uint8_max )
                write_u8( static_cast< mp::mp_u8 >( value ) );
            else if ( value <= limits::uint16_max )
                write_u16( static_cast< mp::mp_u16 >( value ) );
            else if ( value <= limits::uint32_max )
                write_u32( static_cast< mp::mp_u32 >( value ) );
            else
                write_u64( value );

            return *this;
        }

        /**
        * @brief Write a single unsigned byte to the stream and advance the cursor by 1 if the stream has not reached its end.
        * @param value Unsigned byte to write to the stream
        * @return MessagePack&
        */
        MessagePack &write_u8( const mp::mp_u8 value )
        {
            write_raw_value( value, sizeof( mp::mp_u8 ), MPMarker::Uint8 );
            return *this;
        }

        /**
         * @brief Write a single signed byte to the stream and advance the cursor by 1 if the stream has not reached its end.
         * @param value Signed byte to write to the stream
         * @return MessagePack&
         */
        MessagePack &write_i8( const mp::mp_i8 value )
        {
            write_raw_value( value, sizeof( mp::mp_i8 ), MPMarker::Int8 );
            return *this;
        }

        /**
         * @brief Write a `string` of `length` into the MessagePack byte stream.
         * @remark Subtract one to remove the null terminator from the total length
         * @param string Pointer to a C string of `length` characters
         * @param length Size, in bytes, of the string in memory
         * @return MessagePack&
         */
        MessagePack &write_cstr( const mp::mp_u8 *string, const mp::mp_u64 length )
        {
            write_raw_value( reinterpret_cast< mp::mp_u64 >( string ), length, MPMarker::Str8 );
            return *this;
        }

        /**
         * @brief Write a byte array `bytes` of `count` into the MessagePack byte stream.
         * @remark Subtract one to remove the null terminator from the total length
         * @param bytes Pointer to a byte array of `count` bytes
         * @param count Size, in bytes, of the byte array
         * @return MessagePack&
         */
        MessagePack &write_bytes( const mp::mp_u8 *bytes, const mp::mp_u64 count )
        {
            write_raw_value( reinterpret_cast< mp::mp_u64 >( bytes ), count, MPMarker::Bin8 );
            return *this;
        }

        /**
         * @brief Write a marker representing `true` to the stream.
         * @return MessagePack&
         */
        MessagePack &write_true( )
        {
            /*
             * `data` parameter can be ignored in this instance, because the marker represents the value.
             */
            write_raw_value( 0, sizeof( mp_u8 ), MPMarker::True );

            return *this;
        }

        /**
         * @brief Write a marker representing `false` to the stream.
         * @return MessagePack&
         */
        MessagePack &write_false( )
        {
            /*
             * `data` parameter can be ignored in this instance, because the marker represents the value.
             */
            write_raw_value( 0, sizeof( mp_u8 ), MPMarker::False );

            return *this;
        }

        /**
         * @brief Write `true` or `false` to the stream depending on `value`.
         * @param value Boolean value to write
         * @return MessagePack&
         */
        MessagePack &write_boolean( const bool value )
        {
            if ( value )
                write_true ( );
            else
                write_false ( );

            return *this;
        }

        MessagePack &write_fix_ext1( const mp::mp_u8 *byte_array )
        {
            write_raw_value(
                reinterpret_cast< mp::mp_u64 >( byte_array ),
                sizeof( MPFixExt1 ),
                MPMarker::FixExt1
            );

            return *this;
        }

        MessagePack &write_fix_ext2( const mp::mp_u8 *byte_array )
        {
            write_raw_value(
                reinterpret_cast< mp::mp_u64 >( byte_array ),
                sizeof( MPFixExt2 ),
                MPMarker::FixExt2
            );

            return *this;
        }

        MessagePack &write_fix_ext4( const mp::mp_u8 *byte_array )
        {
            write_raw_value(
                reinterpret_cast< mp::mp_u64 >( byte_array ),
                sizeof( MPFixExt4 ),
                MPMarker::FixExt4
            );

            return *this;
        }

        MessagePack &write_fix_ext8( const mp::mp_u8 *byte_array )
        {
            write_raw_value(
                reinterpret_cast< mp::mp_u64 >( byte_array ),
                sizeof( MPFixExt8 ),
                MPMarker::FixExt8
            );

            return *this;
        }

        MessagePack &write_fix_ext16( const mp::mp_u8 *byte_array )
        {
            write_raw_value(
                reinterpret_cast< mp::mp_u64 >( byte_array ),
                sizeof( MPFixExt16 ),
                MPMarker::FixExt16
            );

            return *this;
        }

        /**
         * @brief Check if a marker, typically returned by `decode_single`, denotes the start of a fixext type.
         * @param marker MessagePack marker
         * @return `true` if `marker` is any MessagePack fixext type, `false` otherwise
         */
        bool is_fixext( const MPMarker marker ) const
        {
            return (
                marker == MPMarker::FixExt1 ||
                marker == MPMarker::FixExt2 ||
                marker == MPMarker::FixExt4 ||
                marker == MPMarker::FixExt8 ||
                marker == MPMarker::FixExt16
            );
        }

        /**
         * @brief Check if a marker, typically returned by `decode_single`, denotes the start of an array.
         * @param marker MessagePack marker
         * @return `true` if `marker` is any MessagePack array type, `false` otherwise
         */
        bool is_array( const MPMarker marker ) const
        {
            return (
                marker == MPMarker::FixArray ||
                marker == MPMarker::Array16 ||
                marker == MPMarker::Array32
            );
        }

        /**
         * @brief Check if a marker, typically returned by `decode_single`, denotes an integer.
         * @param marker MessagePack marker
         * @return `true` if `marker` is any MessagePack array type, `false` otherwise
         */
        bool is_integer( const MPMarker marker ) const
        {
            return (
                marker == MPMarker::PosFixInt ||
                marker == MPMarker::NegFixInt ||
                marker == MPMarker::Uint8 ||
                marker == MPMarker::Int8 ||
                marker == MPMarker::Uint16 ||
                marker == MPMarker::Int16 ||
                marker == MPMarker::Uint32 ||
                marker == MPMarker::Int32 ||
                marker == MPMarker::Uint64 ||
                marker == MPMarker::Int64
            );
        }

        /**
         * @brief Decode a single value from the stream, advance the cursor and return the marker denoting its type. If the value cannot be decoded, the destination buffer is too small or the stream is exhausted return `MPMarker::Unused`
         * @remark For any dynamically sized type such as Ext8/16/32, Array16/32 and Map16/32 call this function first to retrieve the marker, size and advance the cursor to the start of the data then call their respective handlers afterward.
         * @return MPMarker denoting the type of the latest value decoded from the stream and its size
         */
        MPDecodeResult decode_single( )
        {
            const auto raw = read_u8 ( );

            MPDecodeResult dr { };

            const auto mk = static_cast< MPMarker >( raw );

            dr.marker = mk;
            dr.size = 0;
            dr.result.as_u64 = 0;

            switch ( dr.marker )
            {
            case MPMarker::PosFixInt: // edge case, handled at the bottom
            case MPMarker::FixMap:    // edge case, handled at the bottom
            case MPMarker::FixArray:  // edge case, handled at the bottom
            case MPMarker::FixStr:    // edge case, handled at the bottom
            case MPMarker::NegFixInt: // edge case, handled at the bottom
                break;
            case MPMarker::Nil:
            case MPMarker::False:
                {
                    dr.size = sizeof( mp_u8 );
                    dr.result.as_bool = false;

                    break;
                }
            case MPMarker::Unused:
                break;
            case MPMarker::True:
                {
                    dr.size = sizeof( mp_u8 );
                    dr.result.as_bool = true;

                    break;
                }
            case MPMarker::Str8:
            case MPMarker::Bin8:
                {
                    dr.size = read_u8 ( );
                    break;
                }
            case MPMarker::Str16:
            case MPMarker::Bin16:
                {
                    dr.size = read_u16 ( );
                    break;
                }
            case MPMarker::Str32:
            case MPMarker::Bin32:
                {
                    dr.size = read_u32 ( );
                    break;
                }
            case MPMarker::Ext8:
                {
                    dr.size = static_cast< mp_u32 >( read_u8 ( ) );
                    break;
                }
            case MPMarker::Ext16:
                {
                    dr.size = static_cast< mp_u32 >( read_u16 ( ) );
                    break;
                }
            case MPMarker::Ext32:
                {
                    dr.size = read_u32 ( );
                    break;
                }
            case MPMarker::Float32:
            case MPMarker::Float64:
                break;
            case MPMarker::Uint8:
                {
                    dr.size = sizeof( mp_u8 );
                    dr.result.as_u8 = read_u8 ( );
                    break;
                }
            case MPMarker::Uint16:
                {
                    dr.size = sizeof( mp::mp_u16 );
                    dr.result.as_u16 = read_u16 ( );
                    break;
                }
            case MPMarker::Uint32:
                {
                    dr.size = sizeof( mp::mp_u32 );
                    dr.result.as_u32 = read_u32 ( );
                    break;
                }
            case MPMarker::Uint64:
                {
                    dr.size = sizeof( mp::mp_u64 );
                    dr.result.as_u64 = sr_.read_u64 ( );
                    break;
                }
            case MPMarker::Int8:
                {
                    dr.size = sizeof( mp_i8 );
                    dr.result.as_i8 = sr_.read_i8 ( );
                    break;
                }
            case MPMarker::Int16:
                {
                    dr.size = sizeof( mp_i16 );
                    dr.result.as_i16 = read_i16 ( );
                    break;
                }
            case MPMarker::Int32:
                {
                    dr.size = sizeof( mp_i32 );
                    dr.result.as_i32 = read_i32 ( );
                    break;
                }
            case MPMarker::Int64:
                {
                    dr.size = sizeof( mp_i64 );
                    dr.result.as_i64 = read_i64 ( );
                    break;
                }
            case MPMarker::FixExt1:
                {
                    // spec: fixext 1 stores an integer and a byte array whose length is 1 byte
                    const auto type_size = 2;

                    dr.size = type_size;
                    dr.result.as_fixext1.type = read_u8 ( );
                    dr.result.as_fixext1.data = read_u8 ( );

                    break;
                }
            case MPMarker::FixExt2:
                {
                    // spec: fixext 2 stores an integer and a byte array whose length is 2 bytes
                    const auto type_size = 3u;

                    dr.size = type_size;
                    dr.result.as_fixext2.type = read_u8 ( );

                    sr_.read( 2, dr.result.as_fixext2.data );

                    break;
                }
            case MPMarker::FixExt4:
                {
                    // spec: fixext 4 stores an integer and a byte array whose length is 4 bytes
                    const auto type_size = 5u;

                    dr.size = type_size;
                    dr.result.as_fixext4.type = read_u8 ( );

                    sr_.read( 4, dr.result.as_fixext4.data );

                    break;
                }
            case MPMarker::FixExt8:
                {
                    // spec: fixext 8 stores an integer and a byte array whose length is 8 bytes
                    const auto type_size = 9u;

                    dr.size = type_size;
                    dr.result.as_fixext8.type = read_u8 ( );

                    sr_.read( 8, dr.result.as_fixext8.data );

                    break;
                }
            case MPMarker::FixExt16:
                {
                    // spec: fixext 16 stores an integer and a byte array whose length is 16 bytes
                    const auto type_size = 17u;

                    dr.size = type_size;
                    dr.result.as_fixext8.type = read_u8 ( );

                    sr_.read( 16, dr.result.as_fixext8.data );

                    break;
                }
            case MPMarker::Map16:
            case MPMarker::Array16:
                {
                    dr.size = static_cast< mp_u32 >( sr_.read_u16 ( ) );
                    break;
                }
            case MPMarker::Map32:
            case MPMarker::Array32:
                {
                    dr.size = sr_.read_u32 ( );
                    break;
                }
            }

            // Edge cases are handled with lower priority to prevent type conflicts.
            if ( !is_fixext( mk ) && dr.size == 0 )
            {
                if ( ( raw & 0x80 ) == 0 )
                {
                    dr.marker = MPMarker::PosFixInt;
                    dr.size = sizeof( mp_u8 );
                    dr.result.as_u8 = static_cast< mp_u8 >( raw & 0x7f );
                }
                else if ( ( raw & 0xe0 ) == 0xe0 )
                {
                    dr.marker = MPMarker::NegFixInt;
                    dr.size = sizeof( mp_i8 );
                    dr.result.as_i8 = static_cast< mp_i8 >( ( raw & 0x1F ) - 0x20 );
                }
                else if ( ( raw & 0x80 ) == 0x80 )
                {
                    dr.marker = MPMarker::FixMap;
                    dr.size = static_cast< mp_u32 >( raw & 0xf );
                }
                else if ( ( raw & 0x90 ) == 0x90 )
                {
                    dr.marker = MPMarker::FixArray;
                    dr.size = static_cast< mp_u32 >( raw & 0xf );
                }
                else if ( ( raw & 0xa0 ) == 0xa0 )
                {
                    dr.marker = MPMarker::FixStr;
                    dr.size = raw & 0x1flu;

                    sr_.read( dr.size, dr.result.as_fixstr );
                }
            }

            return dr;
        }
    };
}
