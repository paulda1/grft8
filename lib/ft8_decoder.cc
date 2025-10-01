#include "ft8_decoder.h"
#include <bitset>

ft8_decoder::ft8_decoder() : d_logger("ft8 decoding") {
  d_logger.info("FT8 decoding constructed");
}
std::bitset<174> 
ft8_decoder::fsk_to_ldpc(const std::vector<int> transmit_symbols) {
  const std::bitset<174> ldpc_bits; 
  const int gray_map[8] = {/*0*/ 000, /*1*/ 001, /*3*/ 010, /*2*/ 011, 
                          /*7*/ 111, /*6*/ 110, /*4*/ 100, /*5*/ 101};

  int len_symbols = transmit_symbols.size();
  for (int i = 0; i < len_symbols; ++i){
    //0-6, 36-42, 72-78
    if ((i >= 0 && i <= 6) || (i >= 36 && i <= 42) || (i >= 72 && 78)){
      continue;
    }

    for (int j=0; j<3; ++j){
      //ldpc_bits[i+j] = transmit_symbols[i]

    }
  } 
  

  
}