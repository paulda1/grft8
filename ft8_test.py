from gnuradio import gr
from gnuradio import blocks
from gnuradio import filter
from gnuradio import ft8
from gnuradio.filter import firdes
import numpy as np

def create_ft8_filtered_signal(message="CQ K1ABC FN42", output_file="ft8_filtered.dat"):
    """
    Create a simple flowgraph that generates filtered FT8 frequency deviations
    """
    tb = gr.top_block()

    sample_rate = 48000
    baud_rate = 6.25
    bt = 2.0
    
    spb = sample_rate / baud_rate 
    ntaps = 64 
    
    encoder = ft8.encoder(message)
    
    gaussian_taps = firdes.gaussian(1.0, spb, bt, ntaps)
    gaussian_filter = filter.fir_filter_fff(1, gaussian_taps)
    
    file_sink = blocks.file_sink(gr.sizeof_float*1, output_file, False)
    
    tb.connect(encoder, gaussian_filter, file_sink)
    
    tb.run()
    
    print(f"Output saved {output_file}")
    
    return gaussian_taps

def import_test():
    try:
        data = np.fromfile("ft8_filtered.dat", dtype=np.float32)
        print(f"Loaded {len(data)} samples")
        print(f"Data range: {data.min():.6f} to {data.max():.6f}")
        print(f"Non-zero samples: {np.count_nonzero(data)}")
        
        if len(data) > 500000:
            print("length matches ft8")
        if np.all(data == 0):
            print("ERROR!!! (All zeroes)")
        else:
            print("SUCCESS: Data contains non-zero values")
    except:
        print("could not read output file")

if __name__ == '__main__':
    create_ft8_filtered_signal()
    import_test()