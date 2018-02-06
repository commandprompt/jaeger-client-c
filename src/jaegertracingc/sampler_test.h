/*
 * Copyright (c) 2018 Uber Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef JAEGERTRACINGC_SAMPLER_TEST_H
#define JAEGERTRACINGC_SAMPLER_TEST_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void test_const_sampler();

void test_probabilistic_sampler();

void test_rate_limiting_sampler();

void test_guaranteed_throughput_probabilistic_sampler();

#ifdef __cplusplus
} /* extern C */
#endif /* __cplusplus */

#endif /* JAEGERTRACINGC_SAMPLER_TEST_H */