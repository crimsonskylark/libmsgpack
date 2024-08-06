#define _WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "mp.hpp"
#include "gtest/gtest.h"

namespace integers
{
    class IntegerFixture : public testing::Test
    {
    protected:
        mp::MessagePack mpack { };
        mp::MPDecodeResult dr { };

        IntegerFixture( )
        {
            const auto buffer = VirtualAlloc(
                nullptr,
                0x200,
                MEM_COMMIT,
                PAGE_READWRITE
            );

            memset( buffer, 0, 0x200 );

            mpack.initialize_streams(
                0,
                0x200,
                static_cast< unsigned char* >( buffer ),
                reinterpret_cast< stream::MemoryReader >( &std::memcpy ),
                reinterpret_cast< stream::MemoryWriter >( &std::memcpy )
            );
        }

        ~IntegerFixture( )
        {
            const auto stream_buf = mpack.stream_buffer ( );

            mpack.reset_all ( );
            VirtualFree( stream_buf, 0, MEM_RELEASE );
        }
    };

    TEST_F( IntegerFixture, TestFixInt )
    {
        EXPECT_EQ( 0x7f, mpack.write_fixint( 0x7f ).decode_single ( ).result.as_u8 );
        EXPECT_EQ( -20, mpack.write_fixint( -20 ).decode_single ( ).result.as_i8 );
        EXPECT_EQ( -1, mpack.write_fixint( -33 ).decode_single( ).result.as_i8 ); // NegFixInt cannot be smaller than -(2^5).
    }

    TEST_F( IntegerFixture, Test8 )
    {
        EXPECT_EQ( 0xa, mpack.write_u8(0xa).decode_single ( ).result.as_u8 );
        EXPECT_EQ( -125, mpack.write_i8(-125).decode_single ( ).result.as_i8 );
    }

    TEST_F( IntegerFixture, Test16 )
    {
        EXPECT_EQ( 0xffff, mpack.write_u16( 0xffff ).decode_single( ).result.as_u16 );
        EXPECT_EQ( -2, mpack.write_i16( 0xfffe ).decode_single( ).result.as_i16 );
    }

    TEST_F( IntegerFixture, Test32 )
    {
        mpack.write_u32( 0xffffffff );
        const auto as_u32 = mpack.decode_single ( ).result.as_u32;

        mpack.write_i32( 0xFFFFFFFE );
        const auto as_i32 = mpack.decode_single ( ).result.as_i32;

        EXPECT_EQ( 0xffffffff, as_u32 );
        EXPECT_EQ( -2, as_i32 );
    }

    TEST_F( IntegerFixture, Test64 )
    {
        mpack.write_u64( 0xffffffffffffffff );
        const auto as_u64 = mpack.decode_single ( ).result.as_u64;

        mpack.write_i64( 0xfffffffffffffffe );
        const auto as_i64 = mpack.decode_single ( ).result.as_i64;

        EXPECT_EQ( 0xffffffffffffffff, as_u64 );
        EXPECT_EQ( -2, as_i64 );
    }
}

namespace fixext
{
    class FixExtFixture : public testing::Test
    {
    protected:
        mp::MessagePack mpack { };
        mp::MPDecodeResult dr { };

        FixExtFixture( )
        {
            const auto buffer = VirtualAlloc(
                nullptr,
                0x200,
                MEM_COMMIT,
                PAGE_READWRITE
            );

            memset( buffer, 0, 0x200 );

            mpack.initialize_streams(
                0,
                0x200,
                static_cast< unsigned char* >( buffer ),
                reinterpret_cast< stream::MemoryReader >( &std::memcpy ),
                reinterpret_cast< stream::MemoryWriter >( &std::memcpy )
            );
        }

        ~FixExtFixture( )
        {
            const auto stream_buf = mpack.stream_buffer ( );
            const auto stream_size = mpack.stream_size ( );

            mpack.reset_all ( );
            VirtualFree( stream_buf, 0, MEM_RELEASE );
        }
    };

    TEST_F( FixExtFixture, TestFixExt1 )
    {
        const unsigned char fix_ext1[ ] = {
            '\xa',
            '\xb'
        };

        mpack.write_fix_ext1( fix_ext1 );

        dr = mpack.decode_single ( );

        EXPECT_EQ( dr.marker, mp::MPMarker::FixExt1 );
        EXPECT_EQ( dr.size, 2 );

        EXPECT_EQ( '\xa', dr.result.as_fixext1.type );
        EXPECT_EQ( '\xb', dr.result.as_fixext1.data );
    }

    TEST_F( FixExtFixture, TestFixExt2 )
    {
        const unsigned char fix_ext2[ ] = {
            '\xa',
            '\xb', '\xc'
        };

        mpack.write_fix_ext2( fix_ext2 );

        dr = mpack.decode_single ( );

        EXPECT_EQ( dr.marker, mp::MPMarker::FixExt2 );
        EXPECT_EQ( dr.size, 3 );

        EXPECT_EQ( '\xa', dr.result.as_fixext2.type );
        EXPECT_EQ( '\xb', dr.result.as_fixext2.data[0] );
        EXPECT_EQ( '\xc', dr.result.as_fixext2.data[1] );
    }

    TEST_F( FixExtFixture, TestFixExt4 )
    {
        const unsigned char valid_fixext4[ ] = {
            '\xa',
            '\xb', '\xc', '\xd', '\xe'
        };

        mpack.write_fix_ext4( valid_fixext4 );

        dr = mpack.decode_single ( );

        EXPECT_EQ( dr.marker, mp::MPMarker::FixExt4 );
        EXPECT_EQ( dr.size, 5 );

        EXPECT_EQ( '\xa', dr.result.as_fixext4.type );

        EXPECT_EQ( '\xb', dr.result.as_fixext4.data[0] );
        EXPECT_EQ( '\xc', dr.result.as_fixext4.data[1] );
        EXPECT_EQ( '\xd', dr.result.as_fixext4.data[2] );
        EXPECT_EQ( '\xe', dr.result.as_fixext4.data[3] );

        mpack.reset_cursors ( );

        memset(
            mpack.stream_buffer ( ),
            0,
            mpack.stream_size ( )
        );

        const unsigned char invalid_fixext4[ ] = {
            '\xa',
            '\xb', '\xc', '\xd', '\xe', '\xf'
        };

        mpack.write_fix_ext4( invalid_fixext4 );

        EXPECT_EQ( mpack.write_cursor( ), 6 );
        EXPECT_EQ( mpack.read_cursor( ), 0 );
    }

    TEST_F( FixExtFixture, TestFixExt8 )
    {
        const unsigned char fix_ext8[ ] = {
            '\xa',                      /* 0 */
            '\xb', '\xc', '\xd', '\xe', /* 4 */
            '\xf', '\xa', '\xb', '\xc'  /* 8 */
        };

        mpack.write_fix_ext8( fix_ext8 );

        EXPECT_EQ( mpack.write_cursor( ), 10 );
        EXPECT_EQ( mpack.read_cursor( ), 0 );

        dr = mpack.decode_single ( );

        EXPECT_EQ( mpack.read_cursor( ), 10 );

        EXPECT_EQ( dr.marker, mp::MPMarker::FixExt8 );
        EXPECT_EQ( dr.size, 9 );

        EXPECT_EQ( '\xa', dr.result.as_fixext8.type );

        EXPECT_EQ( '\xb', dr.result.as_fixext8.data[0] );
        EXPECT_EQ( '\xc', dr.result.as_fixext8.data[1] );
        EXPECT_EQ( '\xd', dr.result.as_fixext8.data[2] );
        EXPECT_EQ( '\xe', dr.result.as_fixext8.data[3] );
        EXPECT_EQ( '\xf', dr.result.as_fixext8.data[4] );
        EXPECT_EQ( '\xa', dr.result.as_fixext8.data[5] );
        EXPECT_EQ( '\xb', dr.result.as_fixext8.data[6] );
        EXPECT_EQ( '\xc', dr.result.as_fixext8.data[7] );
    }

    TEST_F( FixExtFixture, TestFixExt16 )
    {
        const unsigned char fix_ext16[ ] = {
            '\xa',                          /* 0 */
            '\xb', '\xc', '\xd', '\xe',     /* 4 */
            '\xf', '\xa', '\xb', '\xc',     /* 8 */
            '\x6a', '\x7b', '\x5e', '\x3c', /* 12 */
            '\x6b', '\x7c', '\x5f', '\x3d'  /* 16 */
        };

        mpack.write_fix_ext16( fix_ext16 );

        EXPECT_EQ( mpack.write_cursor( ), 18 );
        EXPECT_EQ( mpack.read_cursor( ), 0 );

        dr = mpack.decode_single ( );

        EXPECT_EQ( mpack.read_cursor( ), 18 );

        EXPECT_EQ( dr.marker, mp::MPMarker::FixExt16 );
        EXPECT_EQ( dr.size, 17 );

        EXPECT_EQ( '\xa', dr.result.as_fixext16.type );

        EXPECT_EQ( '\xb', dr.result.as_fixext16.data[0] );
        EXPECT_EQ( '\xc', dr.result.as_fixext16.data[1] );
        EXPECT_EQ( '\xd', dr.result.as_fixext16.data[2] );
        EXPECT_EQ( '\xe', dr.result.as_fixext16.data[3] );
        EXPECT_EQ( '\xf', dr.result.as_fixext16.data[4] );
        EXPECT_EQ( '\xa', dr.result.as_fixext16.data[5] );
        EXPECT_EQ( '\xb', dr.result.as_fixext16.data[6] );
        EXPECT_EQ( '\xc', dr.result.as_fixext16.data[7] );
        EXPECT_EQ( '\x6a', dr.result.as_fixext16.data[8] );
        EXPECT_EQ( '\x7b', dr.result.as_fixext16.data[9] );
        EXPECT_EQ( '\x5e', dr.result.as_fixext16.data[10] );
        EXPECT_EQ( '\x3c', dr.result.as_fixext16.data[11] );
        EXPECT_EQ( '\x6b', dr.result.as_fixext16.data[12] );
        EXPECT_EQ( '\x7c', dr.result.as_fixext16.data[13] );
        EXPECT_EQ( '\x5f', dr.result.as_fixext16.data[14] );
        EXPECT_EQ( '\x3d', dr.result.as_fixext16.data[15] );
    }
}

/**
 * @brief Test `stream::Stream(Reader|Writer)` behaviour.
 */
namespace streams
{
    class StreamFixture : public testing::Test
    {
    protected:
        stream::StreamReader sr { };
        stream::StreamWriter wr { };

        StreamFixture( )
        {
            const auto buffer = VirtualAlloc(
                nullptr,
                0x200,
                MEM_COMMIT,
                PAGE_READWRITE
            );

            memset( buffer, 0, 0x200 );

            sr.set(
                0,
                0x200,
                static_cast< mp::mp_u8* >( buffer ),
                reinterpret_cast< stream::MemoryReader >( memcpy )
            );

            wr.set(
                0,
                0x200,
                static_cast< mp::mp_u8* >( buffer ),
                reinterpret_cast< stream::MemoryWriter >( memcpy )
            );
        }

        ~StreamFixture( )
        {
            const auto stream_size = wr.stream_size ( );
            const auto stream_buf = wr.start ( );

            wr.reset ( );
            sr.reset ( );

            VirtualFree( stream_buf, stream_size, MEM_RELEASE );
        }
    };

    TEST_F( StreamFixture, ReadBytes )
    {
        unsigned char buf[ ] = { 'a', 'b', 'c' };

        wr.write( ARRAYSIZE( buf ), buf );

        EXPECT_EQ( sr.read_u8( ), 'a' );
        EXPECT_EQ( sr.read_u8( ), 'b' );
        EXPECT_EQ( sr.read_u8( ), 'c' );

        wr.clear ( );

        for ( auto index = 0lu; index < 0x200; index++ )
            wr.write_u8( 'a' );

        sr.reset_cursor ( );

        for ( auto index = 0lu; index < 0x200; index++ )
            EXPECT_EQ( sr.read_u8( ), 'a' );
    }

    TEST_F( StreamFixture, ReadOOB )
    {
        /* oob write is tested below */
        for ( auto index = 0lu; index < 0x400; index++ )
            wr.write_u8( 'a' );

        for ( auto index = 0lu; index < 0x400; index++ )
        {
            if ( index < 0x200 )
                EXPECT_EQ( sr.read_u8( ), 'a' );
            else
                EXPECT_EQ( sr.read_u8( ), '\0' ); // sr.read_u8( ) returns 0 for OOB or invalid buffer states

            EXPECT_LE( sr.position( ), sr.stream_size( ) );
        }
    }

    TEST_F( StreamFixture, WriteOverflow )
    {
        for ( auto index = 0lu; index < 0x400; index++ )
            wr.write_u8( 'a' );

        EXPECT_EQ( wr.position( ), wr.stream_size( ) );

        wr.reset_cursor ( );
        wr.clear ( );

        for ( auto index = 0lu; index < 0x100; index++ )
            wr.write_u32( 0xbadf00d );

        EXPECT_EQ( wr.position( ), wr.stream_size( ) );

        wr.reset_cursor ( );
        wr.clear ( );

        for ( auto index = 0lu; index < 0x100; index++ )
            wr.write_u64( 0xbadf00dbadf00d );

        EXPECT_EQ( wr.position( ), wr.stream_size( ) );
    }
}