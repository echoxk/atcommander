#include <mbed.h>
#include <mbed_events.h>

#include <BufferedSoftSerial.h>

#include <fact/iostream.h>

//#define DEBUG_ATC_OUTPUT
// TODO: consider moving these includes into an atcommander folder
#include "hayes.h"
#include "simcom.h"

#define MAL_LED LED1

static void blinky(void) {
    static DigitalOut led(MAL_LED);
    led = !led;
    //printf("LED = %d \r\n",led.read());
}

BufferedSoftSerial serial(PA_10, PB_3);
Serial usb(USBTX, USBRX);

namespace FactUtilEmbedded { namespace std {

ostream cout(usb);
istream cin(usb);
ostream& clog = cout;

}}

using namespace FactUtilEmbedded::std;

streamsize bufferedsoftserial_is_avail(void* ctx)
{
    return ((BufferedSoftSerial*)ctx)->readable();
}

int bufferedsoftserial_getc(void* ctx)
{
    auto stream = (BufferedSoftSerial*)ctx;

    while(!stream->readable()) { Thread::wait(10); }

    return stream->getc();
}

ostream ocserial(serial);
istream icserial(serial, bufferedsoftserial_is_avail, bufferedsoftserial_getc);

static void echo2()
{
    static int counter = 0;

    for(;;)
    {
        if(++counter % 1000 == 0)
        {
            cout << "Heartbeat: " << counter << "\r\n";
        }

        if(icserial.rdbuf()->in_avail())
        {
            int c = icserial.get();
            cout.put(c);
        }
        else if(cin.rdbuf()->in_avail())
        {
            int c = cin.get();
            ocserial.put(c);
        }
        else
        {
            // 10 ms
            Thread::wait(10);
        }
    }
}



int main()
{
    Thread echoThread;

    EventQueue queue;

    clog << "Compiled at " __TIME__ "\r\n";
    clog << "ciserial initialized as serial = " << icserial.rdbuf()->is_serial() << "\r\n";

    serial.baud(9600);

    queue.call_every(1000, blinky);

    ATCommander atc(icserial, ocserial);

    const char ATZ[] = "ATZ";
    const char* keywords_atz[] = { ATZ, "OK", nullptr };
    atc << ATZ;
    atc.send();
    atc.ignore_whitespace_and_newlines();
    if(atc.input_match(keywords_atz) == ATZ)
    {
        clog << "Initialized (was in ATE1 mode)\r\n";
        atc.check_for_ok();
    }

    hayes::v250::echo::command::request(atc, 0);
    hayes::v250::echo::command::read_echo(atc, 0);
    hayes::v250::echo::command::response(atc);

    char buf[128];

    hayes::v250::information::command::request(atc, 0);
    hayes::v250::information::command::response(atc, buf, 128);

    //simcom::generic_at::http_init::command::request(atc);
    //simcom::generic_at::http_init::command::response(atc);

    atc.error.reset();
    atc.command<simcom::generic_at::http_init>();
    if(atc.error.at_result())
    {
        // Then it's probably because http was already initialized, so ignore it
    }
    atc.command<simcom::generic_at::http_para>("URL", "http://vis.lbl.gov/Research/acti/small/small.html");
    simcom::generic_at::http_action::command::request(atc, 0);
    uint16_t status_code;
    uint16_t datalen;
    simcom::generic_at::http_action::command::response(atc, status_code, datalen);

    //atc.command<hayes::standard_at::information2>(0);

    echoThread.start(echo2);

    queue.dispatch();
}
