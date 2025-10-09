/*
 * This file is a translation of a Java file from the Apache Commons RNG.
 *
 * Original Java file:
 * https://github.com/apache/commons-rng/blob/6a1b0c16090912e8fc5de2c1fb5bd8490ac14699/commons-rng-sampling/src/main/java/org/apache/commons/rng/sampling/distribution/RejectionInversionZipfSampler.java
 *
 * Translated to C++ by Gabriel Haas 2024 
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <cmath>
#include <stdexcept>
#include <random>

class RejectionInversionZipfSampler {
private:
    const long numberOfElements;
    const double exponent;
    double hIntegralX1;
    double hIntegralNumberOfElements;
    double s;
    
    std::uniform_real_distribution<double> dis;

public:
    RejectionInversionZipfSampler(long _numberOfElements, 
                                  double _exponent) : numberOfElements(_numberOfElements), exponent(_exponent) {
        if (_numberOfElements <= 0) {
            throw std::invalid_argument("number of elements is not strictly positive: " + std::to_string(_numberOfElements));
        }
        if (_exponent <= 0) {
            //throw std::invalid_argument("exponent is not strictly positive: " + std::to_string(_exponent));
        }

        this->hIntegralX1 = hIntegral(1.5) - 1;
        this->hIntegralNumberOfElements = hIntegral(numberOfElements + 0.5);
        this->s = 2 - hIntegralInverse(hIntegral(2.5) - h(2));
        dis = std::uniform_real_distribution<double>(hIntegralX1, hIntegralNumberOfElements);
    }

    long sample(std::mt19937_64& rng) {
        while(true) {
            double u = 0;
             u = dis(rng);

            double x = hIntegralInverse(u);
            long k = (long)(x + 0.5);

            if (k < 1) {
                k = 1;
            } else if (k > numberOfElements) {
                k = numberOfElements;
            }

            if (k - x <= s || u >= hIntegral(k + 0.5) - h(k)) {
                return k;
            }
        }
    }

    double hIntegral(const double x) {
        const double logX = std::log(x);
        return helper2((1 - exponent) * logX) * logX;
    }

    double h(const double x) {
        return std::exp(-exponent * std::log(x));
    }

    double hIntegralInverse(const double x) {
        double t = x * (1 - exponent);
        if (t < -1) {
            t = -1;
        }
        return std::exp(helper1(t) * x);
    }

    static double helper1(const double x) {
        if (std::abs(x) > 1e-8) {
            return std::log1p(x) / x;
        } else {
            return 1 - x * (0.5 - x * (0.33333333333333333 - 0.25 * x));
        }
    }

    static double helper2(const double x) {
        if (std::abs(x) > 1e-8) {
            return std::expm1(x) / x;
        } else {
            return 1 + x * 0.5 *(1 + x * 0.33333333333333333 * (1 + 0.25 * x));
        }
    }
};