//
// Created by aleksander on 18.04.19.
//

#ifndef CPPPROJ_STDCOMPLEXRENDERER_H
#define CPPPROJ_STDCOMPLEXRENDERER_H


#include <cmath>
#include <vector>
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <complex>

const double RANDOM_MAX = std::mt19937::max();

template<typename real_t>
class BuddhabrotRenderer {
public:
  BuddhabrotRenderer(size_t width, size_t height, size_t max_iter, size_t init_points,
                     void(* func)(std::complex<real_t>&, std::complex<real_t>), real_t random_radius, real_t norm_limit)
      : width(width), height(height), max_iter(max_iter), init_points(init_points), norm_limit(norm_limit),
        rand_min(-random_radius), rand_offset(2 * random_radius),
        orbit_x(max_iter), orbit_y(max_iter), initial(init_points), func(func) {
    std::hash<std::thread::id> hasher;
    random.seed(hasher(std::this_thread::get_id()));
  }

  void setArea(real_t xmid, real_t ymid, real_t factor) {
    this->diff = {width * factor * 2.0 / (width + height), height * factor * 2.0 / (width + height)};
    this->mid = {xmid, ymid};
    this->beg = mid - diff * 0.5;
    this->end = beg + diff;
    this->factor = factor;
  }

  void prepareInitialPoints() {
    for (size_t i = 0; i < init_points; ++i) {
      do {
      } while (!findInitialPointAttempt(initial[i]));
    }
  }

  void outputPointValues(uint32_t* out, size_t iterations) {
    for (size_t i = 0; i < init_points; ++i) {
      computeOrbit(0, initial[i]);
      for (size_t k = 0; k < curr_on_screen; ++k) {
        ++out[orbit_y[k] * width + orbit_x[k]];
      }
      prev_on_screen = curr_on_screen;
      prev_iter = curr_iter;
      prev_contrib = curr_contrib;
      for (size_t j = 0; j < iterations; ++j) {

        std::complex<real_t> x = initial[i];

        mutate(x);
        computeOrbit(0, x);
        if (curr_iter == max_iter || curr_on_screen == 0) {
          continue;
        }
        real_t t1 = transitionProbability(curr_on_screen, prev_on_screen);
        real_t t2 = transitionProbability(prev_on_screen, curr_on_screen);
        real_t alpha = std::min(1.0, std::exp(std::log(curr_contrib * t1) - std::log(prev_contrib * t2)));

        if (alpha > ((real_t) random()) / RANDOM_MAX) {
          prev_on_screen = curr_on_screen;
          prev_iter = curr_iter;
          prev_contrib = curr_contrib;
          initial[i] = x;

          for (size_t k = 0; k < curr_on_screen; ++k) {
            ++out[orbit_y[k] * width + orbit_x[k]];
          }
        }
      }
    }
  }

private:
  inline real_t mapv(real_t value, real_t in_min, real_t in_diff, real_t out_min, real_t out_diff) {
    return (value - in_min) * out_diff / in_diff + out_min;
  }

  real_t norm(real_t a, real_t b) {
    return a * a + b * b;
  }

  void mutateMove(std::complex<real_t>& num) {
    real_t r1 = factor * 0.0001;
    real_t r2 = factor * 0.1;
    real_t phi = (((real_t) random()) / RANDOM_MAX) * M_PI * 2.0;
    real_t r = r2 * std::exp(-std::log(r2 / r1) * (((real_t) random()) / RANDOM_MAX));
    num += std::polar(r, phi);
  }

  void mutateRandom(std::complex<real_t>& num) {
    do {
      num.real(mapv(((real_t) random()) / RANDOM_MAX, 0, 1, rand_min, rand_offset));
      num.imag(mapv(((real_t) random()) / RANDOM_MAX, 0, 1, rand_min, rand_offset));
    } while (std::norm(num) > norm_limit);
  }

  void mutate(std::complex<real_t>& num) {
    if (random() % 5) {
      mutateRandom(num);
    } else {
      mutateMove(num);
    }
  }

  real_t transitionProbability(size_t n1, size_t n2) {
    return (1.0 - ((real_t) (max_iter - n1)) / max_iter) /
           (1.0 - ((real_t) (max_iter - n2)) / max_iter);
  }

  void computeOrbit(std::complex<real_t> a, std::complex<real_t> add) {
    curr_on_screen = 0;
    curr_iter = 0;
    for (size_t j = 0; j < max_iter; ++j) {

      func(a, add);
      if (!std::isfinite(a.real())) {
        throw std::runtime_error("nan detected");
      }

      if (a.real() > beg.real() && a.real() < end.real() && a.imag() > beg.imag() && a.imag() < end.imag()) {
        orbit_x[curr_on_screen] = static_cast<uint16_t>(mapv(a.real(), beg.real(), diff.real(), 0, width));
        orbit_y[curr_on_screen] = static_cast<uint16_t>(mapv(a.imag(), beg.imag(), diff.imag(), 0, height));
        ++curr_on_screen;
      }


      if (std::norm(a) > norm_limit) {
        if (curr_iter == 0) {
          ++curr_iter;
        }
        break;
      }
      ++curr_iter;
    }
    curr_contrib = ((real_t) curr_on_screen) / curr_iter;
  }

  bool findInitialPointAttempt(std::complex<real_t>& num) {
    real_t rand_rad = 2;
    int find_iter = 500, find_iter_2 = 200;
    num = 0;
    for (int i = 0; i < find_iter; ++i) {

      real_t closest = 1e20;
      std::complex<real_t> next = num;

      for (int j = 0; j < find_iter_2; ++j) {

        std::complex<real_t> temp = num;
        real_t phi = (((real_t) random()) / RANDOM_MAX) * M_PI * 2.0;
        real_t r = (((real_t) random()) / RANDOM_MAX) * rand_rad;
        temp += std::polar(r, phi);
        computeOrbit(0, temp);

        if (curr_iter == max_iter) {
          continue;
        }

        if (curr_on_screen > 0) {
          num = temp;
          return true;
        }

        for (size_t k = 0; k < curr_iter; ++k) {
          real_t dist2 = norm(orbit_x[k] - mid.real(), orbit_y[k] - mid.imag());
          if (dist2 < closest) {
            closest = dist2;
            next = temp;
          }
        }

      }
      num = next;
      rand_rad *= 0.5;
    }
    return false;
  }

  size_t width, height, max_iter, init_points;
  std::complex<real_t> beg, end, diff, mid;
  real_t factor;
  real_t norm_limit;
  real_t rand_min;
  real_t rand_offset;
  size_t curr_iter, curr_on_screen, prev_iter, prev_on_screen;
  real_t curr_contrib, prev_contrib;
  std::vector<uint16_t> orbit_x;
  std::vector<uint16_t> orbit_y;
  std::vector<std::complex<real_t>> initial;
  std::mt19937 random;

  void (* func)(std::complex<real_t>&, std::complex<real_t>);
};

#endif //CPPPROJ_STDCOMPLEXRENDERER_H
