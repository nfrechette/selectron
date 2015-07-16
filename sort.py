#1: Initial State: R1 holds elements 1, 3 ...2K -1. R2 holds 2, 4 ...2K.
#2: Output: Element order X in R1 at each step, (the corresponding element order in R2 will be fi(X)), and ?ag array D to record whether changing comparison directions.
#3: for i = 1 to number of steps do
#4: Xi ? R1
#5: . Look ahead next step:
#6: Y ? fi+1(Xi)
#7: . Find intersection(invalid situation):
#8: Z ? Xi nY
#9: . Exchange con?ict elements:
#10: for all x in Z do
#11: . Swap the smaller element(but either is ?ne):
#12: x ? min(x,fi+1(x))
#13: mark Di[x] as changing comparison direction
#14: . exchange with corresponding element in R2:
#15: R1[x] ? fi(x)
#16: end for 17: end fo

def log2(value):
	result = -1
	while value != 0:
		value = value / 2
		result += 1
	return result

def f(positions, step, permutations):
	#result = [0] * len(positions)
	#for i in range(len(positions)):
	#	result[i] = permutations[step - 1][positions[i] - 1]
	#return result
	return permutations[step - 1][:4]

R1 = [1,3,5,7]
R2 = [2,4,6,8]

numElements = len(R1) * 2
numStages = log2(numElements)
numSteps = 0

permutations = []

for i in range(1, numStages + 1):
	numSteps += i
	dist = 2 ** (i - 1)
	#print i
	while dist != 0:
		#print dist
		perm = [0] * numElements
		for j in range(1, numElements + 1):
			period = (j - 1) / dist
			sibling = j + dist
			if (period % 2) == 1:
				sibling = j - dist
			#print '{0} <-> {1}'.format(j, sibling)
			perm[j - 1] = sibling
		print perm
		permutations.append(perm)
		dist = dist / 2
		#print "step done!"

print ""
		
X = [list(R1)]

for i in range(1, numSteps):
	#print i
	Xi = list(R1)
	Y = f(R1, i + 1, permutations)
	#Z = []
	print 'Src R1: {0}'.format(R1)
	for j in range(len(R1)):
		print '{0} wants to compare with {1}'.format(j + 1, Y[j])
		if (j + 1) in R1 and Y[j] in R1:
			Z.append(R1[j])
			print '{0} colliding with {1} in R1!'.format(j + 1, Y[j])
			correction = f([Y[j]], i, permutations)[0]
			print 'swapping {0} for {1}'.format(Y[j], correction)
			R1[j] = correction
	print 'Dst R1: {0}'.format(R1)
	#print '{0}: {1} -> {2} | {3}'.format(i, Xi, Y, Z)
	#for j in range(len(Z)):
	#	x = Z[j]
	#	x = min(x, f([x], i + 1, permutations)[0])
	#	# D..
	#	R1[j] = f([x], i, permutations)[0]
	X.append(list(R1))
	break

#for R1 in X:
	#print R1
	
	
	
	
	
	
	
	
	
	
	