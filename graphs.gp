# Cojocaru Razvan 333CA

# Generate graph for image 1
set term postscript enh color eps
set output "img1.eps"
set multiplot
set xlabel "NUM_SPU"
set ylabel "Time(s)"
set grid
plot 	'./times_comp/img1_s.txt' w l lc rgb "red" title 'Scalar computational',		\
		'./times_comp/img1_v.txt' w l lc rgb "blue" title 'Vectorial computational', 	\
		'./times_total/img1_s.txt' w l lc rgb "green" title 'Scalar total',		\
		'./times_total/img1_v.txt' w l lc rgb "purple" title 'Vectorial total'
unset multiplot

# Generate graph for image 2
set term postscript enh color eps
set output "img2.eps"
set multiplot
set xlabel "NUM_SPU"
set ylabel "Time(s)"
set grid
plot 	'./times_comp/img2_s.txt' w l lc rgb "red" title 'Scalar computational',		\
		'./times_comp/img2_v.txt' w l lc rgb "blue" title 'Vectorial computational', 	\
		'./times_total/img2_s.txt' w l lc rgb "green" title 'Scalar total',		\
		'./times_total/img2_v.txt' w l lc rgb "purple" title 'Vectorial total'
unset multiplot

# Generate graph for image 3
set term postscript enh color eps
set output "img3.eps"
set multiplot
set xlabel "NUM_SPU"
set ylabel "Time(s)"
set grid
plot 	'./times_comp/img3_s.txt' w l lc rgb "red" title 'Scalar computational',		\
		'./times_comp/img3_v.txt' w l lc rgb "blue" title 'Vectorial computational', 	\
		'./times_total/img3_s.txt' w l lc rgb "green" title 'Scalar total',		\
		'./times_total/img3_v.txt' w l lc rgb "purple" title 'Vectorial total'
unset multiplot

# Generate graph for image 4
set term postscript enh color eps
set output "img4.eps"
set multiplot
set xlabel "NUM_SPU"
set ylabel "Time(s)"
set grid
plot 	'./times_comp/img4_s.txt' w l lc rgb "red" title 'Scalar computational',		\
		'./times_comp/img4_v.txt' w l lc rgb "blue" title 'Vectorial computational', 	\
		'./times_total/img4_s.txt' w l lc rgb "green" title 'Scalar total',		\
		'./times_total/img4_v.txt' w l lc rgb "purple" title 'Vectorial total'
unset multiplot
