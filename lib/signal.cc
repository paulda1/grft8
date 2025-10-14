#include "signal.h"
#include <cmath>

Signal::Signal() : d_logger("Signal_Generation") {
    d_logger.info("Signal generation created");
}

// This should return std::vector<float> per header
float
Signal::generate_gaussian_pulse_taps(float t) 
{
  // Implementation depends on what you actually want this to return
  // Based on usage in fsk_tones, it seems like you want a single value
  // So the header should be changed to return float
  float pulse;
  
  float k = M_PI * std::sqrt(2.0f / std::log(2.0f));
  float bt = 2.0f;
  float erf_coeff = k * bt;
  float erf_plus = std::erf(erf_coeff * (t + 0.5f));
  float erf_minus = std::erf(erf_coeff * (t - 0.5f));
  pulse = (erf_plus - erf_minus)/2;        
  
  return pulse; // Return single-element vector, or fix header to return float
}

// This should return float per header  
std::vector<float>
Signal::fsk_tones(std::vector<int> symbols)
{
  const int sample_rate = 12000;
  const float baud_rate = 6.25f;
  const float carrier_frequency = 1000.0f;
  const float amplitude = 1.0f;
  int samples_per_symbol = static_cast<int>(sample_rate / baud_rate);  
  
  const int pulse_span_symbols = 3;
  const int pulse_length = pulse_span_symbols * samples_per_symbol;
  std::vector<float> d_gaussian_pulse(pulse_length);
  
  float start_time = -1.5f;
  for (int i = 0; i < pulse_length; ++i){
    float t = start_time + (static_cast<float>(i)/samples_per_symbol);
    // This call won't work if generate_gaussian_pulse_taps returns vector
    d_gaussian_pulse[i] = generate_gaussian_pulse_taps(t); 
  }
  float max_dgp = static_cast<float>(2 * M_PI/samples_per_symbol);
  
  int signal_length = (symbols.size() * samples_per_symbol) + (2 * samples_per_symbol);
  std::vector<float> freq_deviation(signal_length, 0.0f);
        
  for (int i = 0; i < signal_length; ++i){
    freq_deviation[i] = 2 * M_PI * carrier_frequency / sample_rate;
  }
  for (size_t n = 0; n < symbols.size(); ++n){
    float bn = static_cast<float>(symbols[n]);
    int symbol_start = n * samples_per_symbol;
    for (int i = 0; i < pulse_length; ++i){
      freq_deviation[symbol_start + i] += max_dgp * bn * d_gaussian_pulse[i];
    }
  }
  
  for (int k = 0; k < 2*samples_per_symbol; ++k){
    freq_deviation[k] += max_dgp * d_gaussian_pulse[k + samples_per_symbol] * symbols[0];
    freq_deviation[k + symbols.size() * samples_per_symbol] += max_dgp * d_gaussian_pulse[k] * symbols[symbols.size() - 1];
  }
  
  std::vector<float> fsk_signal;
  int wave_sample_len = symbols.size() * samples_per_symbol;
  fsk_signal.reserve(wave_sample_len);
  float phi = 0;
  for (int i = 0; i < wave_sample_len; ++i) {
      fsk_signal.push_back(amplitude * cosf(phi));
      phi = fmodf(phi + freq_deviation[i + samples_per_symbol], 2 * M_PI);
  }
  
  int samples_ramp = samples_per_symbol / 8;
  for (int i = 0; i < samples_ramp; ++i) {
    float ramp_func = (1 - cosf(2 * M_PI * i / (2 * samples_ramp))) * 0.5f;
    fsk_signal[i] *= ramp_func;
    fsk_signal[wave_sample_len - 1 - i] *= ramp_func;
  }
  
  int samples_15_secs = 15 * sample_rate;
  std::vector<float> padded_signal(samples_15_secs, 0.0f);
  int signal_idx = (samples_15_secs - wave_sample_len) / 2; // Use wave_sample_len, not signal_length
  std::copy(fsk_signal.begin(), fsk_signal.end(), padded_signal.begin() + signal_idx);
  
  d_logger.info("Total number of samples in final signal: {}", padded_signal.size()); // Not ->
  d_logger.info("FSK signal generated successfully");
  return padded_signal;
}
