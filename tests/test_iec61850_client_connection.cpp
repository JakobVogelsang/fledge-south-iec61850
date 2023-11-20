#include <config_category.h>
#include <gtest/gtest.h>
#include <iec61850.hpp>
#include <plugin_api.h>
#include <string.h>
#include "libiec61850/iec61850_server.h"

#include <boost/thread.hpp>
#include <utility>
#include <vector>
#include <libiec61850/hal_thread.h>

using namespace std;

#define TEST_PORT 2404

// PLUGIN DEFAULT PROTOCOL STACK CONF
static string protocol_config = QUOTE({
    "protocol_stack" : {
        "name" : "iec61850client",
        "version" : "0.0.1",
        "transport_layer" : {
            "ied_name" : "IED1",
            "connections" : [
                {
                    "ip_addr" : "127.0.0.1",
                    "port" : 10002
                }
            ],
            "tls" : false
        },
        "application_layer" : {
            "polling_interval" : 0,
            "datasets" : [
                {
                    "dataset_ref" : "simpleIOGenericIO/LLN0.Mags",
                    "entries" : [
                        "simpleIOGenericIO/GGIO1.AnIn1.mag.f[MX]",
                        "simpleIOGenericIO/GGIO1.AnIn2.mag.f[MX]",
                        "simpleIOGenericIO/GGIO1.AnIn3.mag.f[MX]",
                        "simpleIOGenericIO/GGIO1.AnIn4.mag.f[MX]"
                    ],
                    "dynamic" : true
                },
                {
                    "dataset_ref" : "simpleIOGenericIO/LLN0.Events2",
                    "entries" : [
                        "simpleIOGenericIO/GGIO1.AnIn1.mag.f[MX]",
                        "simpleIOGenericIO/GGIO1.AnIn2.mag.f[MX]",
                        "simpleIOGenericIO/GGIO1.AnIn3.mag.f[MX]",
                        "simpleIOGenericIO/GGIO1.AnIn4.mag.f[MX]"
                    ],
                    "dynamic" : false
                }
            ],
            "report_subscriptions" : [
                {
                    "rcb_ref" : "simpleIOGenericIO/LLN0.RP.EventsRCB",
                    "dataset_ref" : "simpleIOGenericIO/LLN0.Mags",
                    "trgops" : [
                        "data_changed",
                        "quality_changed",
                        "gi"
                    ],
                    "gi" : true
                },
                {
                    "rcb_ref" : "simpleIOGenericIO/LLN0.RP.EventsIndexed",
                    "dataset_ref" : "simpleIOGenericIO/LLN0.Events2",
                    "trgops" : [
                        "data_changed",
                        "quality_changed",
                        "gi"
                    ],
                    "gi" : true
                }
            ]
        }
    }
});

// PLUGIN DEFAULT EXCHANGED DATA CONF

static string exchanged_data = QUOTE({
 "exchanged_data": {
  "datapoints": [
   {
    "pivot_id": "TS1",
    "label": "TS1",
    "protocols": [
     {
      "name": "iec61850",
      "objref": "simpleIOGenericIO/GGIO1.SPCSO1",
      "cdc": "SpcTyp"
     }
    ]
   },
   {
    "pivot_id": "TS2",
    "label": "TS2",
    "protocols": [
     {
      "name": "iec61850",
      "objref": "simpleIOGenericIO/GGIO1.SPCSO2",
      "cdc": "SpcTyp"
     }
    ]
   },
   {
    "pivot_id": "TS3",
    "label": "TS3",
    "protocols": [
     {
      "name": "iec61850",
      "objref": "simpleIOGenericIO/GGIO1.SPCSO3",
      "cdc": "SpcTyp"
     }
    ]
   },
   {
    "pivot_id": "TS4",
    "label": "TS4",
    "protocols": [
     {
      "name": "iec61850",
      "objref": "simpleIOGenericIO/GGIO1.SPCSO4",
      "cdc": "SpcTyp"
     }
    ]
   },
   {
    "pivot_id": "TM1",
    "label": "TM1",
    "protocols": [
     {
      "name": "iec61850",
      "objref": "simpleIOGenericIO/GGIO1.AnIn1",
      "cdc": "MvTyp"
     }
    ]
   },
   {
    "pivot_id": "TM2",
    "label": "TM2",
    "protocols": [
     {
      "name": "iec61850",
      "objref": "simpleIOGenericIO/GGIO1.AnIn2",
      "cdc": "MvTyp"
     }
    ]
   },
   {
    "pivot_id": "TM3",
    "label": "TM3",
    "protocols": [
     {
      "name": "iec61850",
      "objref": "simpleIOGenericIO/GGIO1.AnIn3",
      "cdc": "MvTyp"
     }
    ]
   },
   {
    "pivot_id": "TM4",
    "label": "TM4",
    "protocols": [
     {
      "name": "iec61850",
      "objref": "simpleIOGenericIO/GGIO1.AnIn4",
      "cdc": "MvTyp"
     }
    ]
   }
  ]
 }
});

// PLUGIN DEFAULT TLS CONF
static string tls_config = QUOTE({
    "tls_conf" : {
        "private_key" : "server-key.pem",
        "own_cert" : "server.cer",
        "ca_certs" : [
            {
                "cert_file" : "root.cer"
            }
        ]
    }
});

static string tls_config_2 = QUOTE({
    "tls_conf" : {
        "private_key" : "iec104_client.key",
        "own_cert" : "iec104_client.cer",
        "ca_certs" : [
            {
                "cert_file" : "iec104_ca.cer"
            }
        ],
        "remote_certs" : [
            {
                "cert_file" : "iec104_server.cer"
            }
        ]
    }
});

class ConnectionHandlingTest : public testing::Test
{
protected:
    IEC61850 *iec61850 = nullptr;
    int ingestCallbackCalled = 0;
    Reading *storedReading = nullptr;
    int clockSyncHandlerCalled = 0;
    std::vector<Reading *> storedReadings;

    int asduHandlerCalled = 0;
    IedConnection lastConnection = nullptr;
    int lastOA = 0;
    int openConnections = 0;
    int activations = 0;
    int deactivations = 0;
    int maxConnections = 0;

    void SetUp() override
    {
        iec61850 = new IEC61850();

        iec61850->registerIngest(this, ingestCallback);
    }

    void TearDown() override
    {
        iec61850->stop();
        delete iec61850;

        for (auto reading : storedReadings)
        {
            delete reading;
        }
    }

    static bool hasChild(Datapoint &dp, std::string childLabel)
    {
        DatapointValue &dpv = dp.getData();

        auto dps = dpv.getDpVec();

        for (auto sdp : *dps)
        {
            if (sdp->getName() == childLabel)
            {
                return true;
            }
        }

        return false;
    }

    static Datapoint *getChild(Datapoint &dp, std::string childLabel)
    {
        DatapointValue &dpv = dp.getData();

        auto dps = dpv.getDpVec();

        for (Datapoint *childDp : *dps)
        {
            if (childDp->getName() == childLabel)
            {
                return childDp;
            }
        }

        return nullptr;
    }

    static int64_t getIntValue(Datapoint *dp)
    {
        DatapointValue dpValue = dp->getData();
        return dpValue.toInt();
    }

    static std::string getStrValue(Datapoint *dp)
    {
        return dp->getData().toStringValue();
    }

    static bool hasObject(Reading &reading, std::string label)
    {
        std::vector<Datapoint *> dataPoints = reading.getReadingData();

        for (Datapoint *dp : dataPoints)
        {
            if (dp->getName() == label)
            {
                return true;
            }
        }

        return false;
    }

    static Datapoint *getObject(Reading &reading, std::string label)
    {
        std::vector<Datapoint *> dataPoints = reading.getReadingData();

        for (Datapoint *dp : dataPoints)
        {
            if (dp->getName() == label)
            {
                return dp;
            }
        }

        return nullptr;
    }

    static void ingestCallback(void *parameter, Reading reading)
    {
        auto self = (ConnectionHandlingTest *)parameter;

        printf("ingestCallback called -> asset: (%s)\n", reading.getAssetName().c_str());

        std::vector<Datapoint *> dataPoints = reading.getReadingData();

        for (Datapoint *sdp : dataPoints)
        {
            printf("name: %s value: %s\n", sdp->getName().c_str(), sdp->getData().toString().c_str());
        }
        self->storedReading = new Reading(reading);

        self->storedReadings.push_back(self->storedReading);

        self->ingestCallbackCalled++;
    }
};


TEST_F(ConnectionHandlingTest, SingleConnection)
{
    iec61850->setJsonConfig(protocol_config, exchanged_data, tls_config);

    IedModel* model = ConfigFileParser_createModelFromConfigFileEx("../tests/data/simpleIO_direct_control.cfg");

    IedServer server = IedServer_create(model);

    IedServer_start(server,10002);
    iec61850->start();

    Thread_sleep(1000);
 
    ASSERT_TRUE(IedConnection_getState(iec61850->m_client->m_active_connection->m_connection) == IED_STATE_CONNECTED);

    IedServer_stop(server);
    IedServer_destroy(server);
    IedModel_destroy(model);
}