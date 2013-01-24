/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * SLPSATests.cpp
 * Copyright (C) 2013 Simon Newton
 */

#include <ola/Logging.h>
#include <ola/StringUtils.h>
#include <ola/io/BigEndianStream.h>
#include <ola/io/MemoryBuffer.h>
#include <ola/network/IPV4Address.h>
#include <ola/rdm/UID.h>
#include <ola/stl/STLUtils.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "tools/e133/SlpUrlParser.h"
#include "tools/slp/SLPPacketBuilder.h"
#include "tools/slp/SLPPacketConstants.h"
#include "tools/slp/SLPPacketParser.h"
#include "tools/slp/SLPSATestRunner.h"
#include "tools/slp/SLPStrings.h"
#include "tools/slp/ScopeSet.h"
#include "tools/slp/URLEntry.h"
#include "tools/slp/XIDAllocator.h"

using ola::io::BigEndianOutputStream;
using ola::network::IPV4Address;
using ola::rdm::UID;
using ola::slp::LANGUAGE_NOT_SUPPORTED;
using ola::slp::PARSE_ERROR;
using ola::slp::SCOPE_NOT_SUPPORTED;
using ola::slp::SERVICE_REPLY;
using ola::slp::SERVICE_REQUEST;
using ola::slp::SLPPacketBuilder;
using ola::slp::SLP_OK;
using ola::slp::SLP_REQUEST_MCAST;
using ola::slp::ScopeSet;
using ola::slp::ServiceReplyPacket;
using ola::slp::xid_t;
using std::auto_ptr;
using std::vector;

static const char RDMNET_DEVICE_SERVICE[] = "service:rdmnet-device";
static const ScopeSet RDMNET_SCOPES("rdmnet");

/**
 * Build a packet containing length number of 'data' elements.
 */
void BuildNLengthPacket(BigEndianOutputStream *output, uint8_t data,
                        unsigned int length) {
  for (unsigned int i = 0; i < length; i++)
    (*output) << data;
}


/**
 * Try a 0-length UDP packet.
 */
class EmptyPacketTest: public TestCase {
void BuildPacket(BigEndianOutputStream *) {
  SetDestination(UNICAST);
  ExpectTimeout();
}
};
REGISTER_TEST(EmptyPacketTest)

/**
 * Try a UDP packet of length 1.
 */
class SingleByteTest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(MULTICAST);
  ExpectTimeout();
  BuildNLengthPacket(output, 0, 1);
}
};
REGISTER_TEST(SingleByteTest)

/**
 * A SrvRqst for the service rdmnet-device in scope 'rdmnet'
 */
class SrvRqstTest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(MULTICAST);
  ExpectResponse(SERVICE_REPLY);

  SLPPacketBuilder::BuildServiceRequest(output, GetXID(), true, pr_list,
                                        RDMNET_DEVICE_SERVICE, RDMNET_SCOPES);
}

TestState VerifyReply(const uint8_t *data, unsigned int length) {
  ola::io::MemoryBuffer buffer(data, length);
  ola::io::BigEndianInputStream stream(&buffer);

  auto_ptr<const ServiceReplyPacket> reply(
    ola::slp::SLPPacketParser::UnpackServiceReply(&stream));
  if (!reply.get())
    return FAILED;

  if (reply->error_code != SLP_OK) {
    OLA_INFO << "Error code is " << static_cast<int>(reply->error_code);
    return FAILED;
  }

  if (reply->url_entries.size() != 1) {
    OLA_INFO << "Expected 1 URL entry, received "
             << reply->url_entries.size();
    return FAILED;
  }

  const ola::slp::URLEntry &url = reply->url_entries[0];
  OLA_INFO << "Received SrvRply containing " << url;
  const string service = ola::slp::SLPServiceFromURL(url.url());
  if (service != RDMNET_DEVICE_SERVICE) {
    OLA_INFO << "Mismatched SLP service, expected '" << RDMNET_DEVICE_SERVICE
             << "', got '" << service << "'";
    return FAILED;
  }

  IPV4Address remote_ip;
  UID uid(0, 0);
  if (!ParseSlpUrl(url.url(), &uid, &remote_ip)) {
    OLA_INFO << "Failed to extract IP & UID from " << url.url();
    return FAILED;
  }

  if (remote_ip != GetDestinationIP()) {
    OLA_INFO << "IP in url (" << remote_ip
             << ") does not match that of the target";
    return FAILED;
  }
  return PASSED;
}
};
REGISTER_TEST(SrvRqstTest)


/**
 * Empty unicast SrvRqst (just the header)
 */
class EmptyUnicastSrvRqstTest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(UNICAST);
  ExpectError(SERVICE_REPLY, PARSE_ERROR);
  SLPPacketBuilder::BuildSLPHeader(output, SERVICE_REQUEST, 0, 0, GetXID());
}
};
REGISTER_TEST(EmptyUnicastSrvRqstTest)

/**
 * Empty mulitcast SrvRqst (just the header)
 */
class EmptyMulticastSrvRqstTest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(MULTICAST);
  ExpectTimeout();
  SLPPacketBuilder::BuildSLPHeader(output, SERVICE_REQUEST, 0,
                                   SLP_REQUEST_MCAST, GetXID());
}
};
REGISTER_TEST(EmptyMulticastSrvRqstTest)


/**
 * A Unicast SrvRqst with a length longer than the packet
 */
class OverflowUnicastSrvRqstTest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(UNICAST);
  ExpectError(SERVICE_REPLY, PARSE_ERROR);
  SLPPacketBuilder::BuildSLPHeader(output, SERVICE_REQUEST, 30, 0, GetXID());
}
};
REGISTER_TEST(OverflowUnicastSrvRqstTest)

/**
 * A Multicast SrvRqst with a length longer than the packet
 */
class OverflowMulticastSrvRqstTest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(MULTICAST);
  ExpectTimeout();
  SLPPacketBuilder::BuildSLPHeader(output, SERVICE_REQUEST, 30,
                                   SLP_REQUEST_MCAST, GetXID());
}
};
REGISTER_TEST(OverflowMulticastSrvRqstTest)


/**
 * Try a multicast request with the target's IP in the PR List
 */
class SrvRqstPRListTest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(MULTICAST);
  ExpectTimeout();

  pr_list.insert(GetDestinationIP());
  SLPPacketBuilder::BuildServiceRequest(output, GetXID(), true, pr_list,
                                        RDMNET_DEVICE_SERVICE, RDMNET_SCOPES);
}
};
REGISTER_TEST(SrvRqstPRListTest)


/**
 * Try a unicast SrvRqst with a different scope.
 */
class DefaultScopeUnicastTest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(UNICAST);
  ExpectError(SERVICE_REPLY, SCOPE_NOT_SUPPORTED);

  ScopeSet default_scope("default");
  SLPPacketBuilder::BuildServiceRequest(output, GetXID(), false, pr_list,
                                        RDMNET_DEVICE_SERVICE, default_scope);
}
};
REGISTER_TEST(DefaultScopeUnicastTest)


/**
 * Try a multicast SrvRqst with a different scope.
 */
class DefaultScopeMulticastTest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(MULTICAST);
  ExpectTimeout();

  ScopeSet default_scope("default");
  SLPPacketBuilder::BuildServiceRequest(output, GetXID(), true, pr_list,
                                        RDMNET_DEVICE_SERVICE, default_scope);
}
};
REGISTER_TEST(DefaultScopeMulticastTest)


/**
 * Try a unicast SrvRqst with no service-type.
 */
class MissingServiceTypeUnicastRequest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(UNICAST);
  ExpectError(SERVICE_REPLY, PARSE_ERROR);

  SLPPacketBuilder::BuildServiceRequest(output, GetXID(), false, pr_list, "",
                                        RDMNET_SCOPES);
}
};
REGISTER_TEST(MissingServiceTypeUnicastRequest)


/**
 * Try a multicast SrvRqst with no service-type.
 */
class MissingServiceTypeMulticastRequest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(MULTICAST);
  ExpectTimeout();

  SLPPacketBuilder::BuildServiceRequest(output, GetXID(), true, pr_list, "",
                                        RDMNET_SCOPES);
}
};
REGISTER_TEST(MissingServiceTypeMulticastRequest)


/**
 * Try a unicast SrvRqst with a different language.
*/
class NonEnglishUnicastRequest: public TestCase {
void BuildPacket(BigEndianOutputStream *output) {
  SetDestination(UNICAST);
  ExpectError(SERVICE_REPLY, LANGUAGE_NOT_SUPPORTED);

  SLPPacketBuilder::BuildServiceRequest(output, GetXID(), false, pr_list, "",
                                        RDMNET_SCOPES, "fr");
}
};
REGISTER_TEST(NonEnglishUnicastRequest)
