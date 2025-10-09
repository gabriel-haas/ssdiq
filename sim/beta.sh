set -x


g++ -std=c++17 -Wall -Wextra -march=native -O3 zonesim.cpp -o zonesim

SSDFILL=0.9 UNIFORM=1 ALPHA=1 BETA=1 ./zonesim > "betasim-uni.csv" & 

for a in 0.00001 0.0001 0.001 0.01 0.1 1 10; do 
	for b in 0.1 0.5 1 2 5 10 20 50; do 
		#SSDFILL=$f ./zonesim > wa$f.csv &
			SSDFILL=0.9 UNIFORM=0 ALPHA=$a BETA=$b ./zonesim > "betasim$a-$b.csv" & 
		echo $a
	done
done
#SSDFILL=0.9 UNIFORM=0 ALPHA=1 BETA=5 ./zonesim
