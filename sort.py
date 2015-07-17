
def log2(value):
	result = -1
	while value != 0:
		value = value / 2
		result += 1
	return result

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
		perm = []
		for j in range(1, numElements + 1):
			period = (j - 1) / dist
			sibling = j + dist
			if (period % 2) == 1:
				sibling = j - dist
			#print '{0} <-> {1}'.format(j, sibling)
			p = (min(j, sibling), max(j, sibling))
			if not p in perm:
				perm.append(p)
			#perm[j - 1] = (j, sibling)
		print '{0}: {1}'.format(len(permutations) + 1, perm)
		permutations.append(perm)
		dist = dist / 2
		#print "step done!"

# Extra step due to initial swizzle
dist = 2 ** (numStages - 1)
perm = []
for j in range(1, numElements + 1):
	period = (j - 1) / dist
	sibling = j + dist
	if (period % 2) == 1:
		sibling = j - dist
	#print '{0} <-> {1}'.format(j, sibling)
	p = (min(j, sibling), max(j, sibling))
	if not p in perm:
		perm.append(p)
	#perm[j - 1] = (j, sibling)
print 'Extra: {0}'.format(perm)
permutations.append(perm)
		
print ""

for i in range(numSteps - 1):
	print '{0}: input  {1} | {2}'.format(i + 1, R1, R2)
	perm_curr = permutations[i]
	perm_next = permutations[i + 1]
	print perm_curr
	print perm_next
	for p in perm_next:
		src, dst = p
		#print '{0} <-> {1}'.format(src, dst)
		if src in R1 and dst in R1:
			dst_i = R1.index(dst)
			tmp = R2[dst_i]
			print 'Cannot compare {0} and {1}! Swapping {2} with {3}.'.format(src, dst, dst, tmp)
			R1[dst_i] = tmp
			R2[dst_i] = dst
	print '{0}: output {1} | {2}'.format(i + 1, R1, R2)
	print ""

# Extra step
for p in permutations[numSteps]:
	src, dst = p
	if not src in R1:
		src_i = R2.index(src)
		dst_i = R1.index(dst)
		tmp = R1[dst_i]
		print '{0} not in R1! Swapping {1} and {2}.'.format(src, src, tmp)
		R1[dst_i] = src
		R2[src_i] = tmp

print 'output {0} | {1}'.format(R1, R2)
print ""
	
# Merge



def bitonic_sort(up, x):
	if len(x) <= 1:
		return x
	else:
		print 'Sorting: {0}'.format(x)
		first = bitonic_sort(True, x[:len(x) / 2])
		second = bitonic_sort(False, x[len(x) / 2:])
		return bitonic_merge(up, first + second)

def bitonic_merge(up, x): 
	# assume input x is bitonic, and sorted list is returned 
	if len(x) == 1:
		return x
	else:
		print 'Merging: {0}'.format(x)
		bitonic_compare(up, x)
		first = bitonic_merge(up, x[:len(x) / 2])
		second = bitonic_merge(up, x[len(x) / 2:])
		return first + second

def bitonic_compare(up, x):
	dist = len(x) / 2
	for i in range(dist):
		if up:
			print '{0} --> {1}'.format(x[i], x[i + dist])
		else:
			print '{0} <-- {1}'.format(x[i], x[i + dist])
		if (x[i] > x[i + dist]) == up:
			x[i], x[i + dist] = x[i + dist], x[i] #swap

print bitonic_sort(True, [1, 2, 3, 4, 5, 6, 7, 8])




