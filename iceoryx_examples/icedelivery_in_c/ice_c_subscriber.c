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

#include "iceoryx_binding_c/runtime.h"
#include "iceoryx_binding_c/subscriber.h"
#include "iceoryx_binding_c/types.h"
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

void receiving(void)
{
    //! [create runtime instance]
    const char APP_NAME[] = "iox-c-subscriber";
    iox_runtime_init(APP_NAME);
    //! [create runtime instance]

    // When starting the subscriber late it will miss the first samples which the
    // publisher has send. The history ensures that we at least get the last 10
    // samples sent by the publisher when we subscribe.
    //! [create subscriber port]
    iox_sub_options_t options;
    iox_sub_options_init(&options);
    options.historyRequest = 10U;
    options.queueCapacity = 50U;
    //options.queueFullPolicy = QueueFullPolicy_BLOCK_PRODUCER;
    options.nodeName = "iox-c-subscriber-node";
    iox_sub_storage_t subscriberStorage;

    iox_sub_t subscriber = iox_sub_init(&subscriberStorage, "Radar", "FrontLeft", "Object", &options);
    //! [create subscriber port]

    int idx = 0;
    int partzial_idx = 0;

    uint8_t* data = malloc((CHAR_SIZE) * sizeof(uint8_t));
    size_t data_size = 0;

    int position = 17;
    int lost = 0;
    int wrong_order = 0;
    char sub_string[CHAR_SIZE];
    //! [receive and print data]
    struct timespec start, end;
    while (!killswitch)
    {
        if (SubscribeState_SUBSCRIBED == iox_sub_get_subscription_state(subscriber))
        {
            const void* userPayload = NULL;
            // we will receive more than one sample here since the publisher is sending a
            // new sample every 400 ms and we check for new samples only every second
            while (ChunkReceiveResult_SUCCESS == iox_sub_take_chunk(subscriber, &userPayload))
            {
                const struct IceMsg* sample = (const struct IceMsg*)(userPayload);

                memcpy(data + data_size, sample->data, sizeof(sample->data));

                data_size += sizeof(sample->data);
                if (sample->last)
                {
                    /*printf("Received msg %d topic \"Hello\" with \"%.*s\"\n",
                           idx,
                           (int)sample->total_data_length,
                           (char*)data);*/

                    int c = 0;
                    int length = (int)sample->total_data_length - position;

                    // printf("rcv_buf_len %d\n", rcv_buf_len);
                    // printf("position %d\n", position);
                    // printf("length %d\n", length);

                    memset(sub_string, '\0', sizeof(char) * CHAR_SIZE);
                    while (c < length)
                    {
                        sub_string[c] = (char)data[position + c];
                        c++;
                    }
                    sub_string[c] = '\0';

                    int val = atoi(sub_string);
                    if (val > idx)
                    {
                        lost++;
                        idx = val;
                        // printf("Received msg idx %d wait for %d\n", val, idx);
                    }
                    else if (val < idx)
                    {
                        // printf("Received msg idx %d wait for %d\n", val, idx);
                        wrong_order++;
                    }

                    idx++;
                    if ((idx % MSG_CHUNK) == 0)
                    {
                        partzial_idx = 0;
                        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
                        int64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
                        printf("Received msg %d in %ld us msg every %.2f us, msg lost %d, wrong index %d\n",
                               idx,
                               delta_us,
                               ((float)delta_us) / MSG_CHUNK,
                               lost,
                               wrong_order);
                    }


                    if (partzial_idx == 0)
                    {
                        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
                    }
                    partzial_idx++;

                    free(data);
                    data = malloc((CHAR_SIZE) * sizeof(uint8_t));
                    data_size = 0;
                }


                iox_sub_release_chunk(subscriber, userPayload);
            }
        }
        else
        {
            printf("Not subscribed!\n");
        }

        sleep_for(SLEEP_MICRO);
    }
    //! [receive and print data]

    //! [cleanup]
    iox_sub_deinit(subscriber);
    //! [cleanup]
}

int main(void)
{
    printf("start subscriber\n");

    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    receiving();

    printf("end subscriber\n");
    return 0;
}
