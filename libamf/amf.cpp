// amf.cpp:  AMF (Action Message Format) rpc marshalling, for Gnash.
// 
//   Copyright (C) 2005, 2006, 2007, 2008 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//

#ifdef HAVE_CONFIG_H
#include "gnashconfig.h"
#endif

#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>

#include "log.h"
#include "GnashException.h"
#include "buffer.h"
#include "amf.h"
//#include "network.h"
#include "element.h"
#include "amfutf8.h"
#include <boost/cstdint.hpp> // for boost::?int??_t

#if !defined(HAVE_WINSOCK_H) || defined(__OS2__)
# include <sys/types.h>
# include <arpa/inet.h>
#else
# include <windows.h>
# include <io.h>
#endif

using namespace std;
using namespace gnash;

namespace amf 
{

/// \define ENSUREBYTES
///
/// @param from The base address to check.
///
/// @param tooFar The ending address that is one byte too many.
///
/// @param size The number of bytes to check for: from to tooFar.
///
/// @remarks May throw an Exception
#define ENSUREBYTES(from, toofar, size) { \
	if ( from+size >= toofar ) \
		throw ParserException("Premature end of AMF stream"); \
}


/// \brief String representations of AMF0 data types.
///
///	These are used to print more intelligent debug messages.
const char *amftype_str[] = {
    "Number",
    "Boolean",
    "String",
    "Object",
    "MovieClip",
    "Null",
    "Undefined",
    "Reference",
    "ECMAArray",
    "ObjectEnd",
    "StrictArray",
    "Date",
    "LongString",
    "Unsupported",
    "Recordset",
    "XMLObject",
    "TypedObject",
    "AMF3 Data"
};

/// \brief Create a new AMF class.
///	As most of the methods in the AMF class a static, this
///	is primarily only used when encoding complex objects
///	where the byte count is accumulated.
AMF::AMF() 
    : _totalsize(0)
{
//    GNASH_REPORT_FUNCTION;
}

/// Delete the alloczted AMF class
AMF::~AMF()
{
//    GNASH_REPORT_FUNCTION;
}

/// \brief Swap bytes in raw data.
///	This only swaps bytes if the host byte order is little endian.
///
/// @param word The address of the data to byte swap.
///
/// @param size The number of bytes in the data.
///
/// @return A pointer to the raw data.
void *
swapBytes(void *word, size_t size)
{
    union {
	boost::uint16_t s;
	struct {
	    boost::uint8_t c0;
	    boost::uint8_t c1;
	} c;
    } u;
	   
    u.s = 1;
    if (u.c.c0 == 0) {
	// Big-endian machine: do nothing
	return word;
    }

    // Little-endian machine: byte-swap the word

    // A conveniently-typed pointer to the source data
    boost::uint8_t *x = static_cast<boost::uint8_t *>(word);

    /// Handle odd as well as even counts of bytes
    std::reverse(x, x+size);
    
    return word;
}

//
// Methods for encoding data into big endian formatted raw AMF data.
//

/// \brief Encode a 64 bit number to it's serialized representation.
///
/// @param num A double value to serialize.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeNumber(double indata)
{
//    GNASH_REPORT_FUNCTION;
    double num;
    // Encode the data as a 64 bit, big-endian, numeric value
    // only one additional byte for the type
    boost::shared_ptr<Buffer> buf(new Buffer(AMF0_NUMBER_SIZE + 1));
    *buf = Element::NUMBER_AMF0;
    num = indata;
    swapBytes(&num, AMF0_NUMBER_SIZE);
    *buf += num;
    
    return buf;
}

/// \brief Encode a Boolean object to it's serialized representation.
///
/// @param flag The boolean value to serialize.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeBoolean(bool flag)
{
//    GNASH_REPORT_FUNCTION;
    // Encode a boolean value. 0 for false, 1 for true
    boost::shared_ptr<Buffer> buf(new Buffer(2));
    *buf = Element::BOOLEAN_AMF0; 
    *buf += static_cast<boost::uint8_t>(flag);
    
    return buf;
}

/// \brief Encode the end of an object to it's serialized representation.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeObject(const amf::Element &data)
{
    GNASH_REPORT_FUNCTION;
    boost::uint32_t length;
    length = data.propertySize();
    //    log_debug("Encoded data size has %d properties", length);
    boost::shared_ptr<amf::Buffer> buf;
    if (length) {
	buf.reset(new amf::Buffer);
    }
    *buf = Element::OBJECT_AMF0;
    if (data.propertySize() > 0) {
	vector<boost::shared_ptr<amf::Element> >::reverse_iterator ait;
	vector<boost::shared_ptr<amf::Element> > props = data.getProperties();
	for (ait = props.rbegin(); ait != props.rend(); ait++) {
	    boost::shared_ptr<amf::Element> el = (*(ait));
	    boost::shared_ptr<amf::Buffer> item = AMF::encodeElement(el);
	    if (item) {
		*buf += item;
		item.reset();
	    } else {
		break;
	    }
	    //	    el->dump();
	}
    }

    // Terminate the object
    *buf += '\0';
    *buf += '\0';
    *buf += TERMINATOR;

    return buf;
}

/// \brief Encode the end of an object to it's serialized representation.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeObjectEnd()
{
//    GNASH_REPORT_FUNCTION;
    boost::shared_ptr<Buffer> buf(new Buffer(1));
    *buf += TERMINATOR;

    return buf;
}

/// \brief Encode an "Undefined" object to it's serialized representation.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeUndefined()
{
//    GNASH_REPORT_FUNCTION;
    boost::shared_ptr<Buffer> buf(new Buffer(1));
    *buf = Element::UNDEFINED_AMF0;
    
    return buf;
}

/// \brief Encode a "Unsupported" object to it's serialized representation.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeUnsupported()
{
//    GNASH_REPORT_FUNCTION;
    boost::shared_ptr<Buffer> buf(new Buffer(1));
    *buf = Element::UNSUPPORTED_AMF0;
    
    return buf;
}

/// \brief Encode a Date to it's serialized representation.
///
/// @param data A pointer to the raw bytes that becomes the data.
/// 
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeDate(const boost::uint8_t *date)
{
//    GNASH_REPORT_FUNCTION;
//    boost::shared_ptr<Buffer> buf;
    boost::shared_ptr<Buffer> buf(new Buffer(AMF0_NUMBER_SIZE+1));
    *buf = Element::DATE_AMF0;
    double num = *(reinterpret_cast<const double*>(date));
    swapBytes(&num, AMF0_NUMBER_SIZE);
    *buf += num;
    
    return buf;
}

/// \brief Encode a NULL object to it's serialized representation.
///		A NULL object is often used as a placeholder in RTMP.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeNull()
{
//    GNASH_REPORT_FUNCTION;

    boost::shared_ptr<Buffer> buf(new Buffer(1));
    *buf = Element::NULL_AMF0;
    
    return buf;
}

/// \brief Encode an XML object to it's serialized representation.
///
/// @param data A pointer to the raw bytes that becomes the XML data.
/// 
/// @param nbytes The number of bytes to serialize.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeXMLObject(const boost::uint8_t * /*data */, size_t /* size */)
{
//    GNASH_REPORT_FUNCTION;
    boost::shared_ptr<Buffer> buf;
    log_unimpl("XML AMF objects not supported yet");
    buf.reset();
    return buf;
}

/// \brief Encode a Typed Object to it's serialized representation.
///
/// @param data A pointer to the raw bytes that becomes the data.
/// 
/// @param size The number of bytes to serialize.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeTypedObject(const amf::Element &data)
{
    GNASH_REPORT_FUNCTION;
    boost::uint32_t length;
    length = data.propertySize();
//    log_debug("Encoded data size has %d properties", length);
    boost::shared_ptr<amf::Buffer> buf(new amf::Buffer);
    buf->clear();
#if 0
    size_t outsize = 0;
    if (data.getName()) {
	outsize = data.getNameSize() + sizeof(boost::uint16_t);
    } else {
	outsize == 1;
    }
//     if (length <= 0) {
// 	buf.reset();
//  	return buf;
//     }
    //    buf.reset(new amf::Buffer);
#endif
    *buf = Element::TYPED_OBJECT_AMF0;
    // If the name field is set, it's a property, followed by the data
    if (data.getName()) {
	// Add the length of the string for the name of the variable
	size_t namelength = data.getNameSize();
	boost::uint16_t enclength = namelength;
	swapBytes(&enclength, 2);
	*buf += enclength;
	// Now the name itself
	string name = data.getName();
	if (name.size() > 0) {
	    *buf += name;
	}
    }

    if (data.propertySize() > 0) {
	vector<boost::shared_ptr<amf::Element> >::reverse_iterator ait;    
	vector<boost::shared_ptr<amf::Element> > props = data.getProperties();
	for (ait = props.rbegin(); ait != props.rend(); ait++) {
	    boost::shared_ptr<amf::Element> el = (*(ait));
	    boost::shared_ptr<amf::Buffer> item = AMF::encodeElement(el);
	    if (item) {
		*buf += item;
		item.reset();
	    } else {
		break;
	    }
	    //	    el->dump();
	}
    }
    
    // Terminate the object
    *buf += '\0';
    *buf += '\0';
    *buf += TERMINATOR;

    return buf;
}

/// \brief Encode a Reference to an object to it's serialized representation.
///
/// @param data A pointer to the raw bytes that becomes the data.
/// 
/// @param size The number of bytes to serialize.
///
/// @return a binary AMF packet in big endian format (header,data)
boost::shared_ptr<Buffer>
AMF::encodeReference(boost::uint16_t index)
{
//    GNASH_REPORT_FUNCTION;
    boost::uint16_t num = index;
    boost::shared_ptr<amf::Buffer> buf(new Buffer(3));
    *buf = Element::REFERENCE_AMF0;
    swapBytes(&num, sizeof(boost::uint16_t));
    *buf += num;
    
    return buf;
}

/// \brief Encode a Movie Clip (swf data) to it's serialized representation.
///
/// @param data A pointer to the raw bytes that becomes the data.
/// 
/// @param size The number of bytes to serialize.
///
/// @return a binary AMF packet in big endian format (header,data)
boost::shared_ptr<Buffer>
AMF::encodeMovieClip(const boost::uint8_t * /*data */, size_t /* size */)
{
//    GNASH_REPORT_FUNCTION;
    boost::shared_ptr<Buffer> buf;
    log_unimpl("Movie Clip AMF objects not supported yet");
    
    return buf;
}

/// \brief Encode an ECMA Array to it's serialized representation.
///		An ECMA Array, also called a Mixed Array, contains any
///		AMF data type as an item in the array.
///
/// @param data A pointer to the raw bytes that becomes the data.
/// 
/// @param size The number of bytes to serialize.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeECMAArray(const amf::Element &data)
{
//    GNASH_REPORT_FUNCTION;
    boost::uint32_t length;
    bool sparse = false;
    size_t counter = 0;

    length = data.propertySize();
    //    log_debug("Encoded data size has %d properties", length);
    boost::shared_ptr<amf::Buffer> buf(new amf::Buffer);
    if (length == 0) {
	// an undefined array is only 5 bytes, 1 for the type and
	// 4 for the length.
	buf.reset(new amf::Buffer(5));
    }
    *buf = Element::ECMA_ARRAY_AMF0;
    length = 1;
    swapBytes(&length, sizeof(boost::uint32_t));
    *buf += length;

    // At lest for red5, it seems to encode from the last item to the
    // first, so we do the same for now.
    if (data.propertySize() > 0) {
	boost::shared_ptr<amf::Buffer> item;
	vector<boost::shared_ptr<amf::Element> >::const_iterator ait;    
	vector<boost::shared_ptr<amf::Element> > props = data.getProperties();
	for (ait = props.begin(); ait != props.end(); ait++) {
	    boost::shared_ptr<amf::Element> el = (*(ait));
	    if (sparse) {
		sparse = false;
// 		char num[12];
// 		sprintf(num, "%d", counter);
// 		amf::Element elnum(num, el->to_number());
// 		*buf += AMF::encodeElement(elnum);
// 		double nodes = items;
 		amf::Element ellen("length");
		ellen.makeNumber(data.propertySize());
 		*buf += AMF::encodeElement(ellen);
	    } else {
		item = AMF::encodeElement(el);
		if (item) {
		    *buf += item;
		    item.reset();
		} else {
		    break;
		}
	    }
	}
    }

    double count = data.propertySize();
    amf::Element ellen("length", count);
    boost::shared_ptr<amf::Buffer> buflen = ellen.encode();
    *buf += buflen;
    
    // Terminate the object
    *buf += '\0';
    *buf += '\0';
    *buf += TERMINATOR;

    return buf;
}

/// \brief Encode a Long String to it's serialized representation.
///
/// @param data A pointer to the raw bytes that becomes the data.
/// 
/// @param size The number of bytes to serialize.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeLongString(const boost::uint8_t * /* data */, size_t /* size */)
{
//    GNASH_REPORT_FUNCTION;
    boost::shared_ptr<Buffer> buf;
    log_unimpl("Long String AMF objects not supported yet");
    
    return buf;
}

/// \brief Encode a Record Set to it's serialized representation.
///
/// @param data A pointer to the raw bytes that becomes the data.
/// 
/// @param size The number of bytes to serialize.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeRecordSet(const boost::uint8_t * /* data */, size_t /* size */)
{
//    GNASH_REPORT_FUNCTION;
    boost::shared_ptr<Buffer> buf;
    log_unimpl("Reecord Set AMF objects not supported yet");
    
    return buf;
}

/// \brief Encode a Strict Array to it's serialized represenicttation.
///	A Strict Array is one where all the items are the same
///	data type, commonly either a number or a string.
///
/// @param data A pointer to the raw bytes that becomes the data.
/// 
/// @param size The number of bytes to serialize.
///
/// @return a binary AMF packet in big endian format (header,data)
boost::shared_ptr<Buffer>
AMF::encodeStrictArray(const amf::Element &data)
{
    GNASH_REPORT_FUNCTION;
    boost::uint32_t items;
    items = data.propertySize();
    //    log_debug("Encoded data size has %d properties", items);
    boost::shared_ptr<amf::Buffer> buf(new amf::Buffer);
    if (items) {
	buf.reset(new amf::Buffer);
    } else {
	// an undefined array is only 5 bytes, 1 for the type and
	// 4 for the length.
	buf->resize(5);
	//	buf.reset(new amf::Buffer(5));
    }
    *buf = Element::STRICT_ARRAY_AMF0;
    swapBytes(&items, sizeof(boost::uint32_t));
    *buf += items;

    if (data.propertySize() > 0) {
	vector<boost::shared_ptr<amf::Element> >::const_iterator ait;    
	vector<boost::shared_ptr<amf::Element> > props = data.getProperties();
	bool sparse = false;
	size_t counter = 0;
	for (ait = props.begin(); ait != props.end(); ait++) {
	    counter++;
	    boost::shared_ptr<amf::Element> el = (*(ait));
	    // If we see an undefined data item, then switch to an ECMA
	    // array which is more compact. At least this is what Red5 does.
	    if (el->getType() == Element::UNDEFINED_AMF0) {
		if (!sparse) {
		    log_debug("Encoding a strict array as an ecma array");
		    *buf->reference() = Element::ECMA_ARRAY_AMF0;
		    // When returning an ECMA array for a sparsely populated
		    // array, Red5 adds one more to the count to be 1 based,
		    // instead of zero based.
		    boost::uint32_t moreitems = data.propertySize() + 1;
		    swapBytes(&moreitems, sizeof(boost::uint32_t));
		    boost::uint8_t *ptr = buf->reference() + 1;
		    memcpy(ptr, &moreitems, sizeof(boost::uint32_t));
		    sparse = true;
		}
		continue;
	    } else {
		if (sparse) {
		    sparse = false;
		    char num[12];
		    sprintf(num, "%d", counter);
		    amf::Element elnum(num, el->to_number());
		    *buf += AMF::encodeElement(elnum);
		    double nodes = items;
		    amf::Element ellen("length", nodes);
		    *buf += AMF::encodeElement(ellen);
		} else {
		    boost::shared_ptr<amf::Buffer> item = AMF::encodeElement(el);
		    if (item) {
			*buf += item;
			item.reset();
			continue;
		    } else {
			break;
		    }
		}
	    }
// 	    el->dump();
	}
    }
    
    return buf;
}

/// \brief Encode a string to it's serialized representation.
/// 
/// @param str a string value
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeString(const string &str)
{
    boost::uint8_t *ptr = const_cast<boost::uint8_t *>(reinterpret_cast<const boost::uint8_t *>(str.c_str()));
    return encodeString(ptr, str.size());
}

/// \brief Encode a string to it's serialized representation.
/// 
/// @param data The data to serialize into big endian format
/// 
/// @param size The size of the data in bytes
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeString(boost::uint8_t *data, size_t size)
{
//    GNASH_REPORT_FUNCTION;
    boost::shared_ptr<Buffer>buf(new Buffer(size + AMF_HEADER_SIZE));
    *buf = Element::STRING_AMF0;
    // when a string is stored in an element, we add a NULL terminator so
    // it can be printed by to_string() efficiently. The NULL terminator
    // doesn't get written when encoding a string as it has a byte count
    // instead.
    boost::uint16_t length = size;
//    log_debug("Encoded data size is going to be %d", length);
    swapBytes(&length, 2);
    *buf += length;
    buf->append(data, size);
    
    return buf;
}

/// \brief Encode a String object to it's serialized representation.
///	A NULL String is a string with no associated data.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeNullString()
{
//    GNASH_REPORT_FUNCTION;
    boost::uint16_t length;
    
    boost::shared_ptr<Buffer> buf(new Buffer(AMF_HEADER_SIZE));
    *buf = Element::STRING_AMF0;
    // when a string is stored in an element, we add a NULL terminator so
    // it can be printed by to_string() efficiently. The NULL terminator
    // doesn't get written when encoding a string as it has a byte count
    // instead.
    length = 0;
    *buf += length;
    
    return buf;
}

/// \brief Write an AMF element
///
/// This encodes the data supplied to an AMF formatted one. As the
/// memory is allocatd within this function, you *must* free the
/// memory used for each element or you'll leak memory.
///
/// A "packet" (or element) in AMF is a byte code, followed by the
/// data. Sometimes the data is prefixed by a count, and sometimes
/// it's terminated with a 0x09. Ya gotta love these flaky ad-hoc
/// formats.
///
/// All Numbers are 64 bit, big-endian (network byte order) entities.
///
/// All strings are in multibyte format, which is to say, probably
/// normal ASCII. It may be that these need to be converted to wide
/// characters, but for now we just leave them as standard multibyte
/// characters.

/// \brief Encode an Element to it's serialized representation.
///
/// @param el A smart pointer to the Element to encode.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeElement(boost::shared_ptr<amf::Element> el)
{
    return encodeElement(*el);
}

boost::shared_ptr<Buffer>
AMF::encodeElement(const amf::Element& el)
{
//    GNASH_REPORT_FUNCTION;
    boost::shared_ptr<Buffer> buf;
    // Encode the element's data
    switch (el.getType()) {
      case Element::NOTYPE:
	  return buf;
	  break;
      case Element::NUMBER_AMF0:
      {
  //	  boost::shared_ptr<Buffer> encnum = AMF::encodeNumber(el.to_number());
  //	  *buf += encnum;
	  buf = AMF::encodeNumber(el.to_number());
          break;
      }
      case Element::BOOLEAN_AMF0:
      {
// 	  boost::shared_ptr<Buffer> encbool = AMF::encodeBoolean(el.to_bool());
// 	  *buf += encodeBoolean(el.to_bool());
// 	  *buf += encbool;
	  buf = AMF::encodeBoolean(el.to_bool());
          break;
      }
      case Element::STRING_AMF0:
      {
  //	  boost::shared_ptr<Buffer> encstr = AMF::encodeString(el.to_string());
	  //	  *buf += encstr;
	  if (el.getDataSize() == 0) {
	      buf = encodeNullString();
	  } else {
	      buf = encodeString(el.to_string());
	  }
	  break;
      }
      case Element::OBJECT_AMF0:
	  buf = encodeObject(el);
          break;
      case Element::MOVIECLIP_AMF0:
	  buf = encodeMovieClip(el.to_reference(), el.getDataSize());
          break;
      case Element::NULL_AMF0:
	  //	  *buf += Element::NULL_AMF0;
	  buf = encodeNull();
          break;
      case Element::UNDEFINED_AMF0:
	  //	  *buf += Element::UNDEFINED_AMF0;
	  buf = encodeUndefined();
	  break;
      case Element::REFERENCE_AMF0:
	  buf = encodeReference(el.to_short());
          break;
      case Element::ECMA_ARRAY_AMF0:
	  buf = encodeECMAArray(el);
          break;
	  // The Object End gets added when creating the object, so we can just ignore it here.
      case Element::OBJECT_END_AMF0:
	  buf = encodeObjectEnd();
          break;
      case Element::STRICT_ARRAY_AMF0:
      {
	  buf = AMF::encodeStrictArray(el);
          break;
      }
      case Element::DATE_AMF0:
      {
  //	  boost::shared_ptr<Buffer> encdate = AMF::encodeNumber(el.to_number());
	  buf = AMF::encodeDate(el.to_reference());
          break;
      }
      case Element::LONG_STRING_AMF0:
	  buf = encodeLongString(el.to_reference(), el.getDataSize());
          break;
      case Element::UNSUPPORTED_AMF0:
	  buf = encodeUnsupported();
          break;
      case Element::RECORD_SET_AMF0:
	  buf = encodeRecordSet(el.to_reference(), el.getDataSize());
          break;
      case Element::XML_OBJECT_AMF0:
	  buf = encodeXMLObject(el.to_reference(), el.getDataSize());
          // Encode an XML object. The data follows a 4 byte length
          // field. (which must be big-endian)
          break;
      case Element::TYPED_OBJECT_AMF0:
	  buf = encodeTypedObject(el);
          break;
// 	  // This is a Gnash specific value
//       case Element::VARIABLE:
//       case Element::FUNCTION:
//          break;
      case Element::AMF3_DATA:
	  log_error("FIXME: got AMF3 data type");
	  break;
      default:
	  buf.reset();
          break;
    };

    // If the name field is set, it's a property, followed by the data
    boost::shared_ptr<Buffer> bigbuf;
    if (el.getName() && (el.getType() != Element::TYPED_OBJECT_AMF0)) {
	if (buf) {
	    bigbuf.reset(new amf::Buffer(el.getNameSize() + sizeof(boost::uint16_t) + buf->size()));
	} else {
	    bigbuf.reset(new amf::Buffer(el.getNameSize() + sizeof(boost::uint16_t)));
	}
	
	// Add the length of the string for the name of the variable
	size_t length = el.getNameSize();
	boost::uint16_t enclength = length;
	swapBytes(&enclength, 2);
	*bigbuf = enclength;
	// Now the name itself
	string name = el.getName();
	if (name.size() > 0) {
	    *bigbuf += name;
	}
	if (buf) {
	    *bigbuf += buf;
	}
	return bigbuf;
    }
    
    return buf;
}

/// Encode a variable to it's serialized representation.
///
/// @param el A smart pointer to the Element to encode.
///
/// @return a binary AMF packet in big endian format
boost::shared_ptr<Buffer>
AMF::encodeProperty(boost::shared_ptr<amf::Element> el)
{
//    GNASH_REPORT_FUNCTION;
    size_t outsize;
    
    outsize = el->getNameSize() + el->getDataSize() + AMF_PROP_HEADER_SIZE;

    boost::shared_ptr<Buffer> buf(new Buffer(outsize));
    _totalsize += outsize;

    // Add the length of the string for the name of the variable
    size_t length = el->getNameSize();
    boost::uint16_t enclength = length;
    swapBytes(&enclength, 2);
    *buf = enclength;

    if (el->getName()) {
	string name = el->getName();
	if (name.size() > 0) {
	    *buf += name;
	}
    }

    // Add the type of the variable's data
    *buf += el->getType();
    // Booleans appear to be encoded weird. Just a short after
    // the type byte that's the value.
    switch (el->getType()) {
      case Element::BOOLEAN_AMF0:
//  	  enclength = el->to_bool();
//  	  buf->append(enclength);
  	  *buf += el->to_bool();
	  break;
      case Element::NUMBER_AMF0:
	  if (el->to_reference()) {
	      swapBytes(el->to_reference(), AMF0_NUMBER_SIZE);
	      buf->append(el->to_reference(), AMF0_NUMBER_SIZE);
	  }
	  break;
      default:
	  enclength = el->getDataSize();
	  swapBytes(&enclength, 2);
	  *buf += enclength;
	  // Now the data for the variable
	  buf->append(el->to_reference(), el->getDataSize());
    }
    
    return buf;
}

/// \brief Extract an AMF object from an array of raw bytes.
///
/// @param buf A smart pointer to a Buffer to parse the data from.
///
/// @return A smart ptr to an Element.
///
/// @remarks May throw a ParserException
boost::shared_ptr<amf::Element> 
AMF::extractAMF(boost::shared_ptr<Buffer> buf)
{
//    GNASH_REPORT_FUNCTION;
    boost::uint8_t* start = buf->reference();
    boost::uint8_t* tooFar = start+buf->size();
    
    return extractAMF(start, tooFar);
}

/// \brief Extract an AMF object from an array of raw bytes.
///	An AMF object is one of the support data types.
///
/// @param in A real pointer to the raw data to start parsing from.
///
/// @param tooFar A pointer to one-byte-past the last valid memory
///	address within the buffer.
///
/// @return A smart ptr to an Element.
///
/// @remarks May throw a ParserException
boost::shared_ptr<amf::Element> 
AMF::extractAMF(boost::uint8_t *in, boost::uint8_t* tooFar)
{
//    GNASH_REPORT_FUNCTION;

    boost::uint8_t *tmpptr = in;
    boost::uint16_t length;
    boost::shared_ptr<amf::Element> el(new Element);

    if (in == 0) {
        log_error(_("AMF body input data is NULL"));
        return el;
    }

    std::map<boost::uint16_t, amf::Element &> references;
    
    // All elements look like this:
    // the first two bytes is the length of name of the element
    // Then the next bytes are the element name
    // After the element name there is a type byte. If it's a Number type, then
    // 8 bytes are read If it's a String type, then there is a count of
    // characters, then the string value    
    
    // Get the type of the element, which is a single byte.
    // This type casting looks like a stupid mistake, but it's
    // mostly to make valgrind shut up, as it has a tendency to
    // complain about legit code when it comes to all this byte
    // manipulation stuff.
    AMF amf_obj;
    // Jump through hoops to get the type so valgrind stays happy
//    char c = *(reinterpret_cast<char *>(tmpptr));
    Element::amf0_type_e type = static_cast<Element::amf0_type_e>(*tmpptr);
    tmpptr++;                        // skip past the header type field byte

    switch (type) {
      case Element::NUMBER_AMF0:
      {
 	  double swapped = *reinterpret_cast<const double*>(tmpptr);
 	  swapBytes(&swapped, amf::AMF0_NUMBER_SIZE);
 	  el->makeNumber(swapped); 
	  tmpptr += AMF0_NUMBER_SIZE; // all numbers are 8 bit big endian
	  break;
      }
      case Element::BOOLEAN_AMF0:
	  el->makeBoolean(tmpptr);
	  tmpptr += 1;		// sizeof(bool) isn't always 1 for all compilers 
	  break;
      case Element::STRING_AMF0:
	  // get the length of the name
	  length = ntohs((*(boost::uint16_t *)tmpptr) & 0xffff);
	  tmpptr += sizeof(boost::uint16_t);
	  if (length >= SANE_STR_SIZE) {
	      log_error("%d bytes for a string is over the safe limit of %d, line %d",
			length, SANE_STR_SIZE, __LINE__);
	      el.reset();
	      return el;
	  }
//	  log_debug(_("AMF String length is: %d"), length);
	  if (length > 0) {
	      // get the name of the element
	      el->makeString(tmpptr, length);
//	      log_debug(_("AMF String is: %s"), el->to_string());
	      tmpptr += length;
	  } else {
	      el->setType(Element::STRING_AMF0);
	  };
	  break;
      case Element::OBJECT_AMF0:
      {
	  el->makeObject();
	  while (tmpptr < tooFar) { // FIXME: was tooFar - AMF_HEADER_SIZE)
	      if (*tmpptr+3 == TERMINATOR) {
//		  log_debug("No data associated with Property in object");
		  tmpptr++;
		  break;
	      }
	      boost::shared_ptr<amf::Element> child = amf_obj.extractProperty(tmpptr, tooFar); 
	      if (child == 0) {
		  // skip past zero length string (2 bytes), null (1 byte) and end object (1 byte)
//		  tmpptr += 3;
		  break;
	      }
//	      child->dump();
	      el->addProperty(child);
	      tmpptr += amf_obj.totalsize();
	  };
	  tmpptr += AMF_HEADER_SIZE;		// skip past the terminator bytes
	  break;
      }
      case Element::MOVIECLIP_AMF0:
	  log_debug("AMF0 MovieClip frame");
	  break;
      case Element::NULL_AMF0:
	  el->makeNull();
	  break;
      case Element::UNDEFINED_AMF0:
	  el->makeUndefined();
	  break;
      case Element::REFERENCE_AMF0:
      {
	  length = ntohs((*(boost::uint16_t *)tmpptr) & 0xffff);
	  tmpptr += sizeof(boost::uint16_t);
	  el->makeReference(length);
	  // FIXME: connect reference Element to the object pointed to by the index.
	  tmpptr += 3;
	  break;
      }
      // An ECMA array is comprised of any of the data types. Much like an Object,
      // the ECMA array is terminated by the end of object bytes, so parse till then.
      case Element::ECMA_ARRAY_AMF0:
      {
	  el->makeECMAArray();
	  // get the number of elements in the array
	  boost::uint32_t items = ntohl((*(boost::uint32_t *)tmpptr) & 0xffffffff);
	  tmpptr += sizeof(boost::uint32_t);
	  while (tmpptr < (tooFar - AMF_HEADER_SIZE)) {
	      if (*tmpptr == TERMINATOR) {
//		  log_debug("No data associated with Property in object");
		  tmpptr++;
		  break;
	      }
	      boost::shared_ptr<amf::Element> child = amf_obj.extractProperty(tmpptr, tooFar); 
	      if (child == 0) {
		  break;
	      }
//	      child->dump();
	      el->addProperty(child);
	      tmpptr += amf_obj.totalsize();
	  };
	  tmpptr += AMF_HEADER_SIZE;		// skip past the terminator bytes
	  break;
      }
      case Element::OBJECT_END_AMF0:
	  // A strict array is only numbers
	  break;
      case Element::STRICT_ARRAY_AMF0:
      {
	  el->makeStrictArray();
	  // get the number of numbers in the array
	  boost::uint32_t items = ntohl((*(boost::uint32_t *)tmpptr));
	  // Skip past the length field to get to the start of the data
	  tmpptr += sizeof(boost::uint32_t);
	  while (items) {
	      boost::shared_ptr<amf::Element> child = amf_obj.extractAMF(tmpptr, tooFar); 
	      if (child == 0) {
		  break;
	      } else {
// 		  child->dump();
		  el->addProperty(child);
		  tmpptr += amf_obj.totalsize();
		  --items;
	      }
	  };
	  break;
      }
      case Element::DATE_AMF0:
      {
 	  double swapped = *reinterpret_cast<const double*>(tmpptr);
 	  swapBytes(&swapped, amf::AMF0_NUMBER_SIZE);
	  el->makeDate(swapped);
	  tmpptr += AMF0_NUMBER_SIZE; // all dates are 8 bit big endian numbers
	  break;
      }
      case Element::LONG_STRING_AMF0:
	  el->makeLongString(tmpptr);
	  break;
      case Element::UNSUPPORTED_AMF0:
	  el->makeUnsupported(tmpptr);
	  tmpptr += 1;
	  break;
      case Element::RECORD_SET_AMF0:
	  el->makeRecordSet(tmpptr);
	  break;
      case Element::XML_OBJECT_AMF0:
	  el->makeXMLObject(tmpptr);
	  break;
      case Element::TYPED_OBJECT_AMF0:
      {
	  el->makeTypedObject();
	  // a Typed Object has a name
	  length = ntohs((*(boost::uint16_t *)tmpptr) & 0xffff);
	  tmpptr += sizeof(boost::uint16_t);
	  el->setName(tmpptr, length);
	  tmpptr += length;
	  while (tmpptr < tooFar) { // FIXME: was tooFar - AMF_HEADER_SIZE)
	      if (*tmpptr == TERMINATOR) {
//		  log_debug("No data associated with Property in object");
		  tmpptr++;
		  break;
	      }
	      boost::shared_ptr<amf::Element> child = amf_obj.extractProperty(tmpptr, tooFar); 
	      if (child == 0) {
		  // skip past zero length string (2 bytes), null (1 byte) and end object (1 byte)
		  tmpptr += AMF_HEADER_SIZE;
		  break;
	      }
     //	      child->dump();
	      el->addProperty(child);
	      tmpptr += amf_obj.totalsize();
	  };
//	  tmpptr += AMF_HEADER_SIZE;		// skip past the terminator bytes
	  break;
      }
      case Element::AMF3_DATA:
      default:
	  log_unimpl("%s: type %d", __PRETTY_FUNCTION__, (int)type);
	  el.reset();
	  return el;
      }
    
    // Calculate the offset for the next read
    _totalsize = (tmpptr - in);

    return el;
}

/// \brief Extract a Property.
///	A Property is a standard AMF object preceeded by a
///	length and an ASCII name field. These are only used
///	with higher level ActionScript objects.
///
/// @param buf A smart pointer to an Buffer to parse the data from.
///
/// @return A smart ptr to an Element.
///
/// @remarks May throw a ParserException
boost::shared_ptr<amf::Element> 
AMF::extractProperty(boost::shared_ptr<Buffer> buf)
{
//    GNASH_REPORT_FUNCTION;

    boost::uint8_t* start = buf->reference();
    boost::uint8_t* tooFar = start+buf->size();
    return extractProperty(start, tooFar);
}

/// \brief Extract a Property.
///	A Property is a standard AMF object preceeded by a
///	length and an ASCII name field. These are onicly used
///	with higher level ActionScript objects.
///
/// @param in A real pointer to the raw data to start parsing from.
///
/// @param tooFar A pointer to one-byte-past the last valid memory
///	address within the buffer.
///
/// @return A smart ptr to an Element.
///
/// @remarks May throw a ParserException
boost::shared_ptr<amf::Element> 
AMF::extractProperty(boost::uint8_t *in, boost::uint8_t* tooFar)
{
//    GNASH_REPORT_FUNCTION;
    
    boost::uint8_t *tmpptr = in;
    boost::uint16_t length;
    boost::shared_ptr<amf::Element> el;

    length = ntohs((*(boost::uint16_t *)tmpptr) & 0xffff);
    // go past the length bytes, which leaves us pointing at the raw data
    tmpptr += sizeof(boost::uint16_t);

    // sanity check the length of the data. The length is usually only zero if
    // we've gone all the way to the end of the object.

    // valgrind complains if length is unintialized, which as we just set
    // length to a value, this is tottaly bogus, and I'm tired of
    // braindamaging code to keep valgrind happy.
    if (length <= 0) {
 	log_debug("No Property name, object done");
 	return el;
    }
    
    if (length + tmpptr > tooFar) {
	log_error("%d bytes for a string is over the safe limit of %d. Putting the rest of the buffer into the string, line %d", length, SANE_STR_SIZE, __LINE__);
	length = tooFar - tmpptr;
    }    
    
    // name is just debugging help to print cleaner, and should be removed later
//    log_debug(_("AMF property name length is: %d"), length);
    std::string name(reinterpret_cast<const char *>(tmpptr), length);
//    log_debug(_("AMF property name is: %s"), name);
    // Don't read past the end
    if (tmpptr + length < tooFar) {
	tmpptr += length;
    }
    
    char c = *(reinterpret_cast<char *>(tmpptr));
    Element::amf0_type_e type = static_cast<Element::amf0_type_e>(c);
    // If we get a NULL object, there is no data. In that case, we only return
    // the name of the property.
    if (type == Element::NULL_AMF0) {
	log_debug("No data associated with Property \"%s\"", name);
	el.reset(new Element);
	el->setName(name.c_str(), length);
	tmpptr += 1;
	// Calculate the offset for the next read
    } else {
	// process the data with associated with the property.
	// Go past the data to the start of the next AMF object, which
	// should be a type byte.
//	tmpptr += length;
	el = extractAMF(tmpptr, tooFar);
	if (el) {
	    el->setName(name.c_str(), length);
	    tmpptr += totalsize();
	}
    }

    //delete name;
    
    // Calculate the offset for the next read
    _totalsize = (tmpptr - in);

    return el;    
}

} // end of amf namespace

// local Variables:
// mode: C++
// indent-tabs-mode: t
// End:
