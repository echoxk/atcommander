//
// Created by malachi on 3/19/17.
//

#include "modem_3gpp.h"
#include "catch.hpp"

#include "3gpp.h"
#include "simcom.h"

using namespace _3gpp;
using namespace simcom;

TEST_CASE( "3gpp 27.007 simulator tests", "[3gpp-27.007]" )
{
    ATCommander atc(fstd::cin, fstd::cout);

    GIVEN("mobile reporting adjustment")
    {
        atc.command<_27007::mobile_equipment_error>(1);
    }

    GIVEN("network attach")
    {
        atc.command<_27007::attach>(true);
    }
    GIVEN("registration status")
    {
        uint8_t n;
        uint8_t stat;

        atc.status<_27007::registration>(n, stat);
        //_27007::get_registration(atc, n, stat);
    }
    GIVEN("")
    {
        generic_at::set_sms_format(atc, '1');
    }
}
