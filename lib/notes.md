# FT8 Message Transmittor

I first create a top block, using the `gr::make_top_block` line. This handles the entire signal processing pipeline, something to note from the documenation, the maximum number of output items = 100000000. [top_block.h](https://www.gnuradio.org/doc/doxygen/top__block_8h_source.html)

Now for PMTs (Polymorphic types), these carry data from one block/thread to another. If I want to have a pmt string I should use `pmt::string_to_symbol("s")`/`pmt::intern("s")` [PMT](https://wiki.gnuradio.org/index.php/Polymorphic_Types_(PMTs)). Here I convert a sample call sign to the PMT to send as the test message. 

So I need to actually send this test message, the best way for me to do that is by taking the PMT message and using the strobe Message Strobe block to send this every few milliseconds [Message Strobe](https://wiki.gnuradio.org/index.php/Message_Strobe). The two parameters are the PMT message, and the period (ms). For example with 1k chosen for the period, the PMT is sent once per second. 

Now we start connecting blocks, we begin by connecting the strobe block to `msg_processor`[Connect Messages](https://www.gnuradio.org/doc/doxygen/classgr_1_1hier__block2.html#a915d1d5b4b6d8a9db4211d9a4507c955). And `msg_processor` is my custom file for handling PDUs. The make function like the other blocks, returns a shared (smart) pointer to the block. You'll note that there are no input or output signatures `gr::io_signature::make(0, 0, 0)`. And I call the input port for my custom block `Input` and the output `Output`. The `set_msg_handler` function is necessary because when a message arrives at the `Input` port there needs to be a way to handle it, not the comment on lambda functions in the second link [Set Message Handler .h](https://www.gnuradio.org/doc/doxygen/classgr_1_1basic__block.html#a7fed11ec01538bfea999fb37687b43b4) [Message Handler Functions] (https://wiki.gnuradio.org/index.php/Message_Passing). Also, for why the work function is commented out: "blocks that don't have streaming ports usually don't even have a work function." Also, the other steps are the same as before, and ultimately a float vector or the frequency tones are generated and converted to a pdu. To create teh PDU, the freq tones are converted toa uniform vector of 32 bits for the actual data, then the dictionary of metadata has the size of the vector. Then this PDU is published to blocks connected to teh `Output` block. For us this is the `pdu_to_stream`.

For `pdu_to_stream` we use [PDU to Stream](https://www.gnuradio.org/doc/doxygen/namespacegr_1_1pdu.html#a1a3065fd0eabfa46232804e954d8bf3c) EARLY_BURST_APPEND to make sure that as soon as a message comes in from the processor output, to make sure immediately the data is apppended to the output stream. A maximum of 64 PDUs can be queued [To Queue](https://github.com/gnuradio/gnuradio/blob/main/gr-pdu/include/gnuradio/pdu/pdu_to_stream.h). I'm *not very sure* whether this is correct but the input port for `pdu_to_stream` I've guessed as "pdus" based on [same link](https://wiki.gnuradio.org/index.php/Message_Passing), at the bottom part, however I'm usnure if that's the actual name - I can't find the namespace file of `msgport_names::pdus()` to verify what string that is. 

The gaussian pulse tap function, I'm dividing t by samples_per_symbol rather than T = 0.160, changing that right now. I also need to center it, also changing right now ands fixed now.

What the `interp_fir_filter_fff` is take the input from the pdu_to_stream block, interpolates by filling in zeroes however many zeroes as necessary for our sampling rate which is 48k so in our case that comes to 7680. Then you do the 1D convolution with the GFSK taps to get a portion of values of one tone and ... *I have clearly messed up right here, I shall fix this and get back to you*. 

In `multiply_const_ff` the VCO multiplication actually wouldn't make sense because I'm doing it properly right after so deleting that multiplication right now. 




