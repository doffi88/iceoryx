// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2020 - 2022 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "iceoryx_binding_c/publisher.h"
#include "iceoryx_binding_c/runtime.h"
#include "sleep_for.h"
#include "topic_data.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MSG_CHUNK 60000
#define CHAR_SIZE 500
#define SLEEP_MICRO 1000

bool killswitch = false;

static void sigHandler(int signalValue)
{
    // Ignore unused variable warning
    (void)signalValue;
    // caught SIGINT or SIGTERM, now exit gracefully
    killswitch = true;
}


void send_msg(iox_pub_t publisher, uint8_t* data, size_t data_length)
{
    size_t total_data_length = data_length;
    do
    {
        void* userPayload = NULL;
        if (AllocationResult_SUCCESS == iox_pub_loan_chunk(publisher, &userPayload, sizeof(struct IceMsg)))
        {
            struct IceMsg* sample = (struct IceMsg*)userPayload;

            size_t max_data_size = sizeof(sample->data);

            memset(sample->data, '\0', sizeof(sample->data));


            sample->total_data_length = total_data_length;
            sample->first = total_data_length == data_length;
            // send content
            if (data_length > max_data_size)
            {
                memcpy(sample->data, data, max_data_size);
                sample->last = false;
                data_length -= max_data_size;
                data += max_data_size;
            }
            else
            {
                memcpy(sample->data, data, data_length);
                sample->last = true;
                data_length -= data_length;
                data += data_length;
            }


            // printf("%s sent value: %.0f\n", APP_NAME, ct);
            // fflush(stdout);

            iox_pub_publish_chunk(publisher, userPayload);

            // advance plain text buffer according the processed input
        }
        else
        {
            printf("Failed to allocate chunk!");
            break;
        }

    } while (data_length > 0); // repeat till all input is consumed
}

void sending(void)
{
    //! [create runtime instance]
    const char APP_NAME[] = "iox-c-publisher";
    iox_runtime_init(APP_NAME);
    //! [create runtime instance]

    //! [create publisher port]
    iox_pub_options_t options;
    iox_pub_options_init(&options);
    options.historyCapacity = 10U;
    // options.subscriberTooSlowPolicy  = ConsumerTooSlowPolicy_WAIT_FOR_CONSUMER;
    options.nodeName = "iox-c-publisher-node";
    iox_pub_storage_t publisherStorage;

    iox_pub_t publisher = iox_pub_init(&publisherStorage, "Radar", "FrontLeft", "Object", &options);
    //! [create publisher port]

    //! [send and print number]
    int idx = 0;
    int partzial_idx = 0;
    struct timespec start, end;
    while (!killswitch)
    {
        char* snd_s = malloc((CHAR_SIZE) * sizeof(char));
        // send content
        sprintf(snd_s, "HELLO WORLD FROM %d", idx);


        send_msg(publisher, (uint8_t*)snd_s, strlen(snd_s));

        idx++;
        if ((idx % MSG_CHUNK) == 0)
        {
            partzial_idx = 0;
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            int64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
            printf("Published msg num %d in %ld us, msg every %.2f us\n", idx, delta_us, ((float)delta_us) / MSG_CHUNK);
        }

        if (partzial_idx == 0)
        {
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        }
        partzial_idx++;

        free(snd_s);
        sleep_for(SLEEP_MICRO);
    }
    //! [send and print number]

    //! [cleanup]
    iox_pub_deinit(publisher);
    //! [cleanup]
}

int main(void)
{
    printf("start publisher\n");

    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    sending();

    printf("end publisher\n");
    return 0;
}
