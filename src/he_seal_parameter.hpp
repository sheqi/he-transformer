/*******************************************************************************
* Copyright 2017-2018 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#pragma once

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ngraph/log.hpp"
#include "nlohmann/json.hpp"

namespace ngraph
{
    namespace runtime
    {
        namespace he
        {
            class HESealParameter
            {
            public:
                HESealParameter(std::uint64_t poly_modulus,
                                std::uint64_t plain_modulus,
                                std::uint64_t security_level,
                                int fractional_encoder_integer_coeff_count,
                                int fractional_encoder_fraction_coeff_count,
                                std::uint64_t fractional_encoder_base,
                                int evaluation_decomposition_bit_count)
                    : m_poly_modulus(poly_modulus)
                    , m_plain_modulus(plain_modulus)
                    , m_security_level(security_level)
                    , m_fractional_encoder_integer_coeff_count(
                          fractional_encoder_integer_coeff_count)
                    , m_fractional_encoder_fraction_coeff_count(
                          fractional_encoder_fraction_coeff_count)
                    , m_fractional_encoder_base(fractional_encoder_base)
                    , m_evaluation_decomposition_bit_count(evaluation_decomposition_bit_count)
                {
                }

                ~HESealParameter() {}
                // SEALContext
                // Must be 1024, 2048, 4096, 8192, 16384, or 32768, aka n
                std::uint64_t m_poly_modulus;
                std::uint64_t m_plain_modulus;
                // Must be 128 or 192
                std::uint64_t m_security_level;

                // FractionalEncoder
                int m_fractional_encoder_integer_coeff_count;
                int m_fractional_encoder_fraction_coeff_count;
                std::uint64_t m_fractional_encoder_base;

                // generate_evaluation_keys
                int m_evaluation_decomposition_bit_count;
            };

            const static HESealParameter parse_seal_config_or_use_default()
            {
                try
                {
                    const char* config_path = std::getenv("NGRAPH_HE_SEAL_CONFIG");
                    if (config_path != nullptr)
                    {
                        // Read file to string
                        std::ifstream f(config_path);
                        std::stringstream ss;
                        ss << f.rdbuf();
                        std::string s = ss.str();

                        // Parse json
                        nlohmann::json js = nlohmann::json::parse(s);
                        std::uint64_t poly_modulus = js["poly_modulus"];
                        std::uint64_t plain_modulus = js["plain_modulus"];
                        std::uint64_t security_level = js["security_level"];
                        int fractional_encoder_integer_coeff_count =
                            js["fractional_encoder_integer_coeff_count"];
                        int fractional_encoder_fraction_coeff_count =
                            js["fractional_encoder_fraction_coeff_count"];
                        std::uint64_t fractional_encoder_base = js["fractional_encoder_base"];
                        int evaluation_decomposition_bit_count =
                            js["evaluation_decomposition_bit_count"];

                        NGRAPH_INFO << "Using SEAL config for parameters: " << config_path;
                        return HESealParameter(poly_modulus,
                                               plain_modulus,
                                               security_level,
                                               fractional_encoder_integer_coeff_count,
                                               fractional_encoder_fraction_coeff_count,
                                               fractional_encoder_base,
                                               evaluation_decomposition_bit_count);
                    }
                    else
                    {
                        NGRAPH_INFO << "Using SEAL default parameters" << config_path;
                        throw std::runtime_error("config_path is NULL");
                    }
                }
                catch (const std::exception& e)
                {
                    return HESealParameter(8192,      // poly_modulus
                                           2L << 30L, // plain_modulus
                                           128,       // security_level
                                           64,        // fractional_encoder_integer_coeff_count
                                           32,        // fractional_encoder_fraction_coeff_count
                                           2,         // fractional_encoder_base
                                           16         // evaluation_decomposition_bit_count
                                           );
                }
            }
            static HESealParameter default_seal_parameter = parse_seal_config_or_use_default();
        }
    }
}