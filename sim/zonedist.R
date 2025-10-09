library(ggplot2)
library(sqldf)
library(Cairo)
library(gridExtra)
library(reshape)
'%+%' <- function(x, y) paste0(x,y)


#dat= read.csv("zonedistUniform.csv")
#dat= read.csv("zonedist02a1b5.csv")
prefix = "sim"
updates = read.csv(prefix%+%"updates.csv")#, nrows=2000)
updates$type = "udpates"
updatesgc = read.csv(prefix%+%"updatesgc.csv")#, nrows=1000000)
updatesgc$type = "upgc"
zonedist = read.csv(prefix%+%"zonedist.csv")#, nrows=2000)
zonedist$type = "zonedist"
head(updatesgc)
dat = rbind(updates, updatesgc, zonedist)
dat = sqldf("select *, row_number() over (partition by type order by cnt) as rank from dat ")
#sqldf("select min(valid), max(valid), 1.0*min(valid)/ max(valid) from dat")
ggplot(dat, aes(x = rank, y = cnt, color = type)) +
#ggplot(dat, aes(x = rank, y = cnt, color = type)) +
	geom_point(size=0.1) + 
	#geom_hline(yintercept = 0.9, linetype = 'dashed', color= 'gray') +
	#scale_y_log10() + 
	expand_limits(y = 0, x=0) + 
	facet_wrap(~ type, scales="free")
	theme_bw()

dev.new()

######################################################################################################
######################################################################################################

# Generate data for different alpha and beta values
beta_density <- function(alpha, beta, x) {
  dbeta(x, alpha, beta)
}
x_vals <- seq(0, 0.5, by = 0.001)
data <- data.frame(x = rep(x_vals, 4),
                   y = c(beta_density(0.001, 1, x_vals),
                         beta_density(0.001, 5, x_vals),
                         beta_density(0.001, 10, x_vals),
                         beta_density(0.001, 50, x_vals)),
                   group = factor(rep(1:4, each = length(x_vals))))
# Plot using ggplot2
ggplot(data, aes(x = x, y = y, color = group)) +
  geom_line() +
  scale_color_discrete(name = "Params (alpha, beta)") +
  labs(title = "Beta Distribution for Different Alpha and Beta Values",
       x = "X Values",
       y = "Density",
       caption = "Computed using 'dbeta' function in R") +
  theme_minimal()

  sort(as.integer(rbeta(10000, 0.000100, 0.100000) * 1e9))


######################################################################################################
######################################################################################################


# facet over rep
dat = read.csv("betafull.csv")
head(dat)
#dat = sqldf("select * from dat do where rep = (select max(rep) from dat di where do.uniform = di.uniform and do.alpha = di.alpha and do.beta = do.beta)")
ggplot(dat, aes(x = rep, y = WA)) +
	geom_line() + 
	expand_limits(y = 0) + 
	facet_grid(-beta ~ alpha) +
	theme_bw()

dev.new()

# MAX
dat = sqldf("select * from dat join (select max(rep) as rep, uniform, alpha, beta from dat group by uniform, alpha, beta) using (uniform, alpha, beta, rep)")
head(dat)
ggplot(dat, aes(x = factor(alpha), y = factor(beta), color=1/WA, label=round(WA, 2))) +
	geom_label() + 
	scale_color_gradientn(colours = terrain.colors(4), trans="log10") +
	expand_limits(y = 0) + 
	theme_bw()

dat= read.csv("dist.csv")
head(dat)
#dat = sqldf("select *, rank() over (order by valid) as rank, 1.0 * valid / (select max(valid) from dat) as perc from dat ")
dat = sqldf("select * from dat where cnt != 0")
options(scipen=10000)
ggplot(dat, aes(x = val, y = cnt)) +
	geom_line() + 
	geom_hline(yintercept = 0.9, linetype = 'dashed', color= 'gray') +
	scale_y_log10() + 
	expand_limits(y = 1) + 
	theme_bw()
