//-----------------------------------------------------------------------------
// Filename: mjpeg.h
//
// Description: Minimal header only implementation of mjpeg based on RFC2435 and
// the JPEG File Interchange Format (JFIF):
// http://www.ecma-international.org/publications/files/ECMA-TR/ECMA%20TR-098.pdf
//
// Author(s):
// Aaron Clauson (aaron@sipsorcery.com)
//
// History:
// 26 May 2020	Aaron Clauson	  Created, Dublin, Ireland.
//
// License and Attributions:
// Licensed under the MIT license.
// * Some work derived from ffmpeg (GPL > 2.1) but fairly substantive
// changes have been made to the original source.
//-----------------------------------------------------------------------------

#ifndef MJPEG_H
#define MJPEG_H

#include <winsock2.h>
#include <iostream>
#include <stdint.h>
#include <stdexcept>
#include <vector>

// Need to do lots of implicit int conversions.
#pragma warning(push)
#pragma warning(disable : 4242 4244)

namespace mjpeg
{
    /**
    * Writes a 16 bit unsigned integer to a network buffer (both the RTP and JPEG
    * headers use a Big Endian on-wire format as does the JFIF header).
    * @param[in] buf: the network buffer to write to.
    * @param[in] posn: the start position in the buffer to write the integer to.
    */
    void static inline write_u16(std::vector<uint8_t>& buf, uint16_t val)
    {
        buf.push_back(val >> 8 & 0xff);
        buf.push_back(val & 0xff);
    }

    /**
    * Extracts a 16 bit unsigned integer from a network buffer.
    * @param[in] buf: the network buffer holding the byes received from the wire.
    * @param[in] posn: the start position in the buffer to extract the integer from.
    * @@Returns a 16 bit unsigned integer.
    */
    uint16_t static inline read_u16(std::vector<uint8_t>& buf, int posn)
    {
        auto data = buf.data();
        return (uint16_t)data[posn] << 8 & 0xff00 |
                (uint16_t)data[posn + 1] & 0x00ff;
    }

    /**
    * Extracts 3 bytes (24 bits) representing a 32 bit unsigned integer from
    * a network buffer.
    * @param[in] buf: the network buffer holding the byes received from the wire.
    * @param[in] posn: the start position in the buffer to extract the integer from.
    * @@Returns a 32 bit unsigned integer.
    */
    uint32_t static inline read_u24(std::vector<uint8_t>& buf, int posn)
    {
        auto data = buf.data();
        return (uint32_t)data[posn + 2] << 16 & 0x00ff0000 |
               (uint32_t)data[posn + 1] << 8 & 0x0000ff00 |
               (uint32_t)data[posn] & 0x000000ff;
    }

    /**
    * Extracts a 32 bit unsigned integer from a network buffer.
    * @param[in] buf: the network buffer holding the byes received from the wire.
    * @param[in] posn: the start position in the buffer to extract the integer from.
    * @@Returns a 32 bit unsigned integer.
    */
    uint32_t static inline read_u32(std::vector<uint8_t>& buf, int posn)
    {
        auto data = buf.data();
        return (uint32_t)data[posn] << 24 & 0xff000000 |
               (uint32_t)data[posn + 1] << 16 & 0x00ff0000 |
               (uint32_t)data[posn + 2] << 8 & 0x0000ff00 |
               (uint32_t)data[posn + 3] & 0x000000ff;
    }

    /**
    * Minimal 12 byte Real-time Protocol (RTP) header as defined in
    * https://tools.ietf.org/html/rfc3550.
    * The RTP header is the first bit of data in the UDP packets carrying the mjpeg stream.
    * Limitations: No facility for extensions, no facility comprehension of CSRC lists
    * (this class is designed to deal with a single media stream with a single sender
    * and receiver).
    */
    class RtpHeader
    {
    public:
        static const int RTP_VERSION = 2;
        static const int RTP_MINIMUM_HEADER_LENGTH = 12;
        static const int INVALID_HEADER_ERROR_CODE = -1;

        uint8_t Version = RTP_VERSION; // protocol version: 2 bits.
        uint8_t PaddingFlag = 0; // padding flag: 1 bit.
        uint8_t HeaderExtensionFlag = 0; // header extension flag: 1 bit.
        uint8_t CSRCCount = 0; // CSRC count: 4 bits.
        uint8_t MarkerBit = 0; // marker bit: 1 bit.
        uint16_t PayloadType = 0; // payload type: 7 bits.
        uint16_t SeqNum = 0; // sequence number: 16 bits.
        uint32_t Timestamp = 0; // timestamp: 32 bits.
        uint32_t SyncSource = 0; // synchronization source: 32 bits.

        /**
        * Deserialises an RTP header from a network buffer.
        * @param[in] buf: the network buffer holding the byes received from the wire.
        * @param[in] posn: the start position in the buffer to extract the RTP header from.
        * @@Returns if successful the length of the deserialised RTP header. If not successful an
        * negative integer error code.
        */
        int Deserialise(std::vector<uint8_t>& buf, int posn)
        {
            if (buf.size() - posn < RTP_MINIMUM_HEADER_LENGTH)
            {
                std::cerr << "Error parsing mjpeg header, the available buffer size was less than the minimum RTP header length." << std::endl;
                return INVALID_HEADER_ERROR_CODE;
            }
            else
            {
                auto rawBuffer = buf.data() + posn;
                Version = rawBuffer[0] >> 6 & 0x03;
                PaddingFlag = rawBuffer[0] >> 5 & 0x01;
                HeaderExtensionFlag = rawBuffer[0] >> 4 & 0x01;
                CSRCCount = rawBuffer[0] & 0x0f;
                MarkerBit = rawBuffer[1] >> 7 & 0x01;
                PayloadType = rawBuffer[1] & 0x7f;
                SeqNum = read_u16(buf, posn + 2);
                Timestamp = read_u32(buf, posn + 4);
                SyncSource = read_u32(buf, posn + 8);

                return RTP_MINIMUM_HEADER_LENGTH;
            }
        }
    };

    /*
      Minimal RTP JPEG header class as specified in:
      https://tools.ietf.org/html/rfc2435#appendix-B.

      JPEG header

          0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       | Type-specific |              Fragment Offset                  |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |      Type     |       Q       |     Width     |     Height    |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

      Note: Restart markers are not yet supported. A Type field of
      between 64 and 127 indicate restart markers are being used.

      JPEG Quantization RTP header allow inband quantization tables.
      Will be present for the first packet in a JPEG frame (offset 0) if
      Q is in the range 128-255.

      From: https://tools.ietf.org/html/rfc2435#appendix-B

      Quantization Table header

        0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |      MBZ      |   Precision   |             Length            |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                    Quantization Table Data                    |
     |                              ...                              |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
    class JpegRtpHeader
    {
    public:
        static const int JPEG_MIN_HEADER_LENGTH = 8;
        static const int JPEG_DEFAULT_TYPE_SPECIFIER = 0;
        static const int JPEG_TYPE_RESTART_MARKER_START = 64;
        static const int JPEG_TYPE_RESTART_MARKER_END = 127;
        static const int JPEG_QUANTIZATION_HEADER_LENGTH = 4;
        static const int Q_TABLE_INBAND_MINIMUM = 128;
        static const int INVALID_HEADER_ERROR_CODE = -1;

        uint8_t TypeSpecifier; // type-specific field: 8 bits.
        uint32_t Offset; // fragment byte offset: 24 bits.
        uint8_t Type; // id of jpeg decoder params: 8 bits.
        uint8_t Q; // quantization factor (or table id): 8 bits. Values 128 to 255 indicate Quantization header in use.
        uint8_t Width; // frame width in 8 pixel blocks. 8 bits.
        uint8_t Height; // frame height in 8 pixel blocks. 8 bits.

        // Quantization Table header is only included in the first RTP packet of each frame.
        uint8_t Mbz{ 0 }; //
        uint8_t Precision{ 0 };
        uint16_t Length{ 0 }; // the length in bytes of the quantization table data to follow: 16 bits
        std::vector<uint8_t> QTable; // Quantization table.

        /**
        * Deserialises a JPEG RTP header from a network buffer.
        * @param[in] buf: the network buffer holding the byes received from the wire.
        * @param[in] posn: the start position in the buffer to extract the JPEG RTP header from.
        * @@Returns if successful the length of the deserialised JPEG RTP header. If not successful an
        * negative integer error code.
        */
        int Deserialise(std::vector<uint8_t>& buf, int posn)
        {
            if (buf.size() - posn < JPEG_MIN_HEADER_LENGTH)
            {
                std::cerr << "Error parsing mjpeg header, the available buffer size was less than the minimum JPEG RTP header length." << std::endl;
                return INVALID_HEADER_ERROR_CODE;
            }
            else
            {
                int headerLength = JPEG_MIN_HEADER_LENGTH;

                auto rawBuffer = buf.data() + posn;
                TypeSpecifier = rawBuffer[0];
                Offset = read_u24(buf, posn + 1);
                Type = rawBuffer[4];
                Q = rawBuffer[5];
                Width = rawBuffer[6];
                Height = rawBuffer[7];

                // Check that the JPEG payload can be interpreted.
                if (TypeSpecifier != JPEG_DEFAULT_TYPE_SPECIFIER)
                {
                    std::cerr << "Error parsing mjpeg header, non-default RTP JPEG type specifiers are not yet supported." << std::endl;
                    return INVALID_HEADER_ERROR_CODE;
                }
                else if (Type >= JPEG_TYPE_RESTART_MARKER_START && Type <= JPEG_TYPE_RESTART_MARKER_END)
                {
                    std::cerr << "Error parsing mjpeg header, JPEG restart markers are not yet supported." << std::endl;
                    return INVALID_HEADER_ERROR_CODE;
                }

                // Inband Q tables are only included in the first RTP packet in the frame.
                if (Offset == 0 && Q >= Q_TABLE_INBAND_MINIMUM)
                {
                    if (buf.size() - posn - JPEG_MIN_HEADER_LENGTH < JPEG_QUANTIZATION_HEADER_LENGTH)
                    {
                        std::cerr << "Error parsing mjpeg header, The available buffer size was less than the minimum JPEG RTP header length." << std::endl;
                        return INVALID_HEADER_ERROR_CODE;
                    }
                    else
                    {
                        Mbz = rawBuffer[8];
                        Precision = rawBuffer[9];
                        Length = read_u16(buf, posn + 10);

                        headerLength += JPEG_QUANTIZATION_HEADER_LENGTH + Length;

                        if (Length > 0)
                        {
                            if (buf.size() - posn - JPEG_MIN_HEADER_LENGTH - JPEG_QUANTIZATION_HEADER_LENGTH < Length)
                            {
                                std::cerr << "Error parsing mjpeg header, the available buffer size is shorter than the JPEG Quantization header length." << std::endl;
                                return INVALID_HEADER_ERROR_CODE;
                            }
                            else
                            {
                                // Copy the Quantization table from the network buffer.
                                std::copy(buf.begin() + posn + JPEG_MIN_HEADER_LENGTH + JPEG_QUANTIZATION_HEADER_LENGTH,
                                          buf.begin() + posn + JPEG_MIN_HEADER_LENGTH + JPEG_QUANTIZATION_HEADER_LENGTH + Length,
                                          std::back_inserter(QTable));
                            }
                        }
                    }
                }

                return headerLength;
            }
        }
    };

    /***
      JPEG File Interchange Format (JFIF) header.
    */
    class Jfif
    {
    public:
        /*
        JPEG marker codes
        Derived from https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/mjpeg.h
        */
        enum JpegMarker
        {
            /* start of frame */
            SOF0 = 0xc0, /* baseline */
            SOF1 = 0xc1, /* extended sequential, huffman */
            SOF2 = 0xc2, /* progressive, huffman */
            SOF3 = 0xc3, /* lossless, huffman */

            SOF5 = 0xc5, /* differential sequential, huffman */
            SOF6 = 0xc6, /* differential progressive, huffman */
            SOF7 = 0xc7, /* differential lossless, huffman */
            JPG = 0xc8, /* reserved for JPEG extension */
            SOF9 = 0xc9, /* extended sequential, arithmetic */
            SOF10 = 0xca, /* progressive, arithmetic */
            SOF11 = 0xcb, /* lossless, arithmetic */

            SOF13 = 0xcd, /* differential sequential, arithmetic */
            SOF14 = 0xce, /* differential progressive, arithmetic */
            SOF15 = 0xcf, /* differential lossless, arithmetic */

            DHT = 0xc4, /* define huffman tables */

            DAC = 0xcc, /* define arithmetic-coding conditioning */

            APP0 = 0xe0,

            SOI = 0xd8, /* start of image */
            EOI = 0xd9, /* end of image */
            SOS = 0xda, /* start of scan */
            DQT = 0xdb, /* define quantization tables */
            DNL = 0xdc, /* define number of lines */
            DRI = 0xdd, /* define restart interval */
            DHP = 0xde, /* define hierarchical progression */
            EXP = 0xdf, /* expand reference components */
        };

        // Huffman tables taken from:
        // https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/jpegtables.c

        /* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
        /* IMPORTANT: these are only valid for 8-bit data precision! */
        const uint8_t mjpeg_bits_dc_luminance[17] = { /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
        const uint8_t mjpeg_val_dc[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

        const uint8_t mjpeg_bits_dc_chrominance[17] = { /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };

        const uint8_t mjpeg_bits_ac_luminance[17] = { /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
        const uint8_t mjpeg_val_ac_luminance[162] = { 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa };

        const uint8_t mjpeg_bits_ac_chrominance[17] = { /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };

        const uint8_t mjpeg_val_ac_chrominance[162] = { 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa };

        /*
        Create the Huffman table for the JFIF header.
        Derived from https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/rtpdec_jpeg.c
        */
        static int jpeg_create_huffman_table(std::vector<uint8_t>& p, int table_class, int table_id, const uint8_t* bits_table, const uint8_t* value_table)
        {
            int i, n = 0;

            p.push_back(table_class << 4 | table_id);

            for (i = 1; i <= 16; i++)
            {
                n += bits_table[i];
                p.push_back(bits_table[i]);
            }

            for (i = 0; i < n; i++)
            {
                p.push_back(value_table[i]);
            }
            return n + 17;
        }

        static void jpeg_put_marker(std::vector<uint8_t>& p, int code)
        {
            p.push_back(0xff);
            p.push_back(code);
        }

        void jpeg_create_header(std::vector<uint8_t>& buf, uint32_t type, uint32_t w, uint32_t h, const uint8_t* qtable, int nb_qtable, int dri)
        {
            /* Convert from blocks to pixels. */
            w <<= 3;
            h <<= 3;

            /* SOI */
            jpeg_put_marker(buf, SOI);

            /* JFIF header */
            jpeg_put_marker(buf, APP0);
            write_u16(buf, 16);
            buf.push_back('J');
            buf.push_back('F');
            buf.push_back('I');
            buf.push_back('F');
            buf.push_back(0);
            write_u16(buf, 0x0201);
            buf.push_back(0);
            write_u16(buf, 1);
            write_u16(buf, 1);
            buf.push_back(0);
            buf.push_back(0);

            if (dri)
            {
                jpeg_put_marker(buf, DRI);
                write_u16(buf, 4);
                write_u16(buf, dri);
            }

            /* DQT */
            jpeg_put_marker(buf, DQT);
            write_u16(buf, 2 + nb_qtable * (1 + 64));

            for (int i = 0; i < nb_qtable; i++)
            {
                buf.push_back(i);

                /* Each table is an array of 64 values given in zig-zag
                 * order, identical to the format used in a JFIF DQT
                 * marker segment. */
                std::copy(qtable, qtable + 64, std::back_inserter(buf));
            }

            /* DHT */
            jpeg_put_marker(buf, DHT);
            // Record the position the length of the huffman tables needs to be written.
            size_t sizePosn = buf.size();
            // The size isn't known yet. It will be set once the tables have been written.
            write_u16(buf, 0);

            int dht_size = 2;
            dht_size += jpeg_create_huffman_table(buf, 0, 0, mjpeg_bits_dc_luminance, mjpeg_val_dc);
            dht_size += jpeg_create_huffman_table(buf, 0, 1, mjpeg_bits_dc_chrominance, mjpeg_val_dc);
            dht_size += jpeg_create_huffman_table(buf, 1, 0, mjpeg_bits_ac_luminance, mjpeg_val_ac_luminance);
            dht_size += jpeg_create_huffman_table(buf, 1, 1, mjpeg_bits_ac_chrominance, mjpeg_val_ac_chrominance);
            //AV_WB16(dht_size_ptr, dht_size);
            buf.at(sizePosn) = dht_size >> 8 & 0xff;
            buf.at(sizePosn + 1) = dht_size & 0xff;

            /* SOF0 */
            jpeg_put_marker(buf, SOF0);
            write_u16(buf, 17); /* size */
            buf.push_back(8); /* bits per component */
            write_u16(buf, h);
            write_u16(buf, w);
            buf.push_back(3); /* number of components */
            buf.push_back(1); /* component number */
            buf.push_back((2 << 4) | (type ? 2 : 1)); /* hsample/vsample */
            buf.push_back(0); /* matrix number */
            buf.push_back(2); /* component number */
            buf.push_back(1 << 4 | 1); /* hsample/vsample */
            buf.push_back(nb_qtable == 2 ? 1 : 0); /* matrix number */
            buf.push_back(3); /* component number */
            buf.push_back(1 << 4 | 1); /* hsample/vsample */
            buf.push_back(nb_qtable == 2 ? 1 : 0); /* matrix number */

            /* SOS */
            jpeg_put_marker(buf, SOS);
            write_u16(buf, 12);
            buf.push_back(3);
            buf.push_back(1);
            buf.push_back(0);
            buf.push_back(2);
            buf.push_back(17);
            buf.push_back(3);
            buf.push_back(17);
            buf.push_back(0);
            buf.push_back(63);
            buf.push_back(0);
        }
    };

}

#pragma warning(pop)

#endif // MJPEG_H
