source("duck.R")
library(ggplot2)
library(grid)
library(gridExtra)
library(sqldf)

a = duck("select column0, count(*) as cnt
	 FROM read_csv('/Users/gabrielhaas/ssdiq/traces/traces_RocksDBYCSB_input_traces.txt')
	 ---FROM read_csv('/Users/gabrielhaas/ssdiq/traces/traces_MSRCambridge_input_traces.txt')
	 ---FROM read_csv('/Users/gabrielhaas/ssdiq/traces/traces_FIU_input_traces.txt')
	 ---FROM read_csv('/Users/gabrielhaas/ssdiq/traces/traces_Alibaba_input_traces.txt')
	 group by column0
	 order by cnt desc
	 limit 1000000")
a$id = seq(1, nrow(a))



head(a)

ggplot(a, aes(x=id, y=cnt)) +
	geom_point() +
	scale_y_log10() +
	expand_limits(y=1) +
	theme_bw()


a = duck("select column0, count(*) as cnt
	 FROM read_csv('/Users/gabrielhaas/ssdiq/traces/traces_RocksDBYCSB_input_traces.txt')
	 ---FROM read_csv('/Users/gabrielhaas/ssdiq/traces/traces_MSRCambridge_input_traces.txt')
	 ---FROM read_csv('/Users/gabrielhaas/ssdiq/traces/traces_FIU_input_traces.txt')
	 ---FROM read_csv('/Users/gabrielhaas/ssdiq/traces/traces_Alibaba_input_traces.txt')
	 group by column0
	 order by cnt desc
	 ")
