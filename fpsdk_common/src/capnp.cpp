/**
 * \verbatim
 * ___    ___
 * \  \  /  /
 *  \  \/  /   Copyright (c) Fixposition AG (www.fixposition.com) and contributors
 *  /  /\  \   License: see the LICENSE file
 * /__/  \__\
 * \endverbatim
 *
 * @file
 * @brief Fixposition SDK: Cap'n Proto utilities
 */

/* LIBC/STL */

/* EXTERNAL */
#if FP_USE_CAPNP
#  include <capnp/serialize-packed.h>
#  include <capnp/serialize.h>
#endif

/* PACKAGE */
#include "fpsdk_common/capnp.hpp"

namespace fpsdk {
namespace common {
namespace capnp {
/* ****************************************************************************************************************** */

#if FP_USE_CAPNP

std::vector<uint8_t> MessageToUnpacked(::capnp::MessageBuilder& capnp_msg)
{
    const kj::Array<::capnp::word> words = ::capnp::messageToFlatArray(capnp_msg);
    const kj::ArrayPtr<const kj::byte> bytes = words.asBytes();
    return { bytes.begin(), bytes.begin() + bytes.size() };
}

// ---------------------------------------------------------------------------------------------------------------------

std::vector<uint8_t> MessageToPacked(::capnp::MessageBuilder& capnp_msg)
{
    kj::VectorOutputStream stream;
    ::capnp::writePackedMessage(stream, capnp_msg);
    const kj::ArrayPtr<const kj::byte> array = stream.getArray();
    return { array.begin(), array.begin() + array.size() };
}

#endif  // FP_USE_CAPNP

/* ****************************************************************************************************************** */
}  // namespace capnp
}  // namespace common
}  // namespace fpsdk
