# CellBE-Compression
Lossy image compresion designed for the Cell Broadband Engine architecture.

<b>PPU code:</br></b>
The code running on the central PPU contains the compress_parallel and decompress_parallel functions, with similar implementation.
The SPU structures that will be sent through DMA are alligned at 16 bytes and then the addresses are sent to the corresponding processing units through the mailbox.
The image data is split into block-lines, sized BLOCK_SIZE * img_width. Each SPU receives a number of these block-lines for processing, and the first SPU receives less data in case the number of block-lines is not an even multiple of the number of SPUs.

<b>SPU code:</br></b>
The control structure address is obtained through mailbox and the actual values through DMA.
Data processing is done on one block-line at a time, given the limited available memory of each SPU.
There are 4 different functions that perform scalar compression/decompression and vectorial compression/decompression.
Vectorial functions use native Cell vectors, effectively performing 4 simultaneous operations.

<b>Performance analysis:</br></b>
All tests have been performed on a CellBE emulator.
A few observations after performing measurements:
- the graph shape is identical, the image size is directly proportional to the computing time
- for a large lumber of SPUs used, I/O impact becomes considerable
- computing performance is doubled with adding two times as many SPUs
The performance improvement is dramatic, with computing time reduced to 0.76s compared to the serial time of 10.25s.
Taking into account I/O, total time has been reduced to 3.90s from 13.50s.
