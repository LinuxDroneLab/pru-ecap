#include <stdint.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <pru_rpmsg.h>
#include <pru_ecap.h>
#include <string.h>
#include "resource_table.h"

volatile register unsigned __R31;

#define VIRTIO_CONFIG_S_DRIVER_OK       4

unsigned char payload[RPMSG_BUF_SIZE];

/**
 * main.c
 */
int main(void)
{
    struct pru_rpmsg_transport transport;
    unsigned short src, dst, len;
    volatile unsigned char *status;

    /* Allow OCP master port access by the PRU so the PRU can read external memories */
    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

    /* Clear the status of the PRU-ICSS system event that the ARM will use to 'kick' us */
    CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

    /* Make sure the Linux drivers are ready for RPMsg communication */
    /* this is another place where a hang could occur */
    status = &resourceTable.rpmsg_vdev.status;
    while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK))
        ;

    /* Initialize the RPMsg transport structure */
    /* this function is defined in rpmsg_pru.c.  It's sole purpose is to call pru_virtqueue_init twice (once for
     vring0 and once for vring1).  pru_virtqueue_init is defined in pru_virtqueue.c.  It's sole purpose is to
     call vring_init.  Not sure yet where that's defined, but it appears to be part of the pru_rpmsg iface.*/
    /* should probably test for RPMSG_SUCCESS.  If not, then the interface is not working and code should halt */
    pru_rpmsg_init(&transport, &resourceTable.rpmsg_vring0,
                   &resourceTable.rpmsg_vring1, TO_ARM_HOST, FROM_ARM_HOST);

    /* Create the RPMsg channel between the PRU and ARM user space using the transport structure. */
    // In a real-time environment, rather than waiting forever, this can probably be run loop-after-loop
    // until success is achieved.  At that point, set a flag and then enable the send/receive functionality
    while (pru_rpmsg_channel(RPMSG_NS_CREATE, &transport, CHAN_NAME, CHAN_DESC,
    CHAN_PORT) != PRU_RPMSG_SUCCESS)
        ;

    // Disabilito ed azzero interrupts
    CT_ECAP.ECEINT = 0x00;
    CT_ECAP.ECCTL2 &= EC_STOP_MSK; // Stop ecap
    CT_ECAP.ECCLR &= ECCLR_MSK;

    // Abilito interrupts
    CT_ECAP.ECEINT = ECEINT_CFG;

    // Configure & start ecap
    CT_ECAP.ECCTL1 = ECCTL1_CFG;
    CT_ECAP.ECCTL2 = ECCTL2_CFG & EC_STOP_MSK;

    uint8_t active = 0;

    struct EcapData
    {
        char cmd[8];
        uint32_t cap1[8];
        uint32_t cap2[8];
        uint32_t cap3[8];
        uint32_t cap4[8];
    };
    uint32_t counter = 0;

    struct EcapData *result = (struct EcapData *) payload;
    strcpy(result->cmd, "DATA");
//    result->cap1;
//    result->cap2;
//    result->cap3;
//    result->cap4;

    while (1)
    {
        if (active)
        {
            counter %= 8;
            if (CT_ECAP.ECFLG & ECFLG_MSK)
            //if (counter > 100000L)
            {
                if (CT_ECAP.ECFLG & 0x0002)
                {
                    strcpy(result->cmd, "DATA");
                    result->cap1[counter] = CT_ECAP.CAP1_bit.CAP1;
                    result->cap2[counter] = CT_ECAP.CAP2_bit.CAP2;
                    result->cap3[counter] = CT_ECAP.CAP3_bit.CAP3;
                    result->cap4[counter] = CT_ECAP.CAP4_bit.CAP4;
                    CT_ECAP.ECCLR |= ECCLR_MSK; // remove EVT4 interrupt and INT
                    counter++;
                    if(counter == 8) {
                        pru_rpmsg_send(&transport, dst, src, payload,
                                           sizeof(struct EcapData));
                    }
                }
//                if (CT_ECAP.ECFLG & 0x0004)
//                {
//                    result->cap2 = CT_ECAP.CAP2_bit.CAP2;
//                }
//                if (CT_ECAP.ECFLG & 0x0008)
//                {
//                    result->cap3 = CT_ECAP.CAP3_bit.CAP3;
//                }
//                if (CT_ECAP.ECFLG & 0x0010)
//                {
//                    result->cap4 = CT_ECAP.CAP4_bit.CAP4;
//                }
            }
        }
        if (__R31 & HOST_INT)
        {
            if (pru_rpmsg_receive(&transport, &src, &dst, payload,
                                  &len) == PRU_RPMSG_SUCCESS)
            {
                int eq = strncmp("START", (const char *) payload, 5);
                if (eq == 0)
                {
                    active = 1;
                    CT_ECAP.TSCTR = 0x00000000;
                    CT_ECAP.ECCTL2 = ECCTL2_CFG; // start ecap
                    pru_rpmsg_send(&transport, dst, src, "STARTED", 8);
                    CT_ECAP.ECFRC |= 0x0010; // force EVT4 (test scope)
                }
                else if (eq < 0)
                {
                    active = 0;
                    CT_ECAP.ECCTL2 = ECCTL2_CFG & EC_STOP_MSK; // stop ecap
                    pru_rpmsg_send(&transport, dst, src, "MINOR", 6);
                }
                else if (eq > 0)
                {
                    active = 0;
                    CT_ECAP.ECCTL2 = ECCTL2_CFG & EC_STOP_MSK; // stop ecap
                    pru_rpmsg_send(&transport, dst, src, "MAJOR", 6);
                }
            }
            else
            {
                CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;
            }
        }
    }
}
