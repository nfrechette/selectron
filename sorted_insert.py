

def gt_mask(a, b):
	result = [0] * len(a)
	for i in range(len(a)):
		if a[i] > b[i]: result[i] = -1
		else: result[i] = 0
	return result

def lt_mask(a, b):
	result = [0] * len(a)
	for i in range(len(a)):
		if a[i] < b[i]: result[i] = -1
		else: result[i] = 0
	return result
	
def replicate(a, width):
	return [a] * width

def select(a, b, mask):
	result = [0] * len(a)
	for i in range(len(a)):
		if mask[i] == 0: result[i] = a[i]
		else: result[i] = b[i]
	return result

def add(a, b):
	result = [0] * len(a)
	for i in range(len(a)):
		result[i] = a[i] + b[i]
	return result

def mul(a, b):
	result = [0] * len(a)
	for i in range(len(a)):
		result[i] = a[i] * b[i]
	return result

def dot(a, b):
	result = a[0] * b[0]
	for i in range(1, len(a)):
		result += a[i] * b[i]
	return result
	
def hsum2(a):
	return dot(a, replicate(1, len(a)))

def hsum(a):
	# 0 + 4, 1 + 5, 2 + 6, 3 + 7	4 + 0, 5 + 1, 6 + 2, 7 + 3
	tmp1 = add(a, shuffle(a, a, [4,5,6,7,0,1,2,3]))
	# 0 + 2, 1 + 3		2 + 0, 3 + 1	4 + 6, 5 + 7	6 + 4, 7 + 5
	tmp2 = add(tmp1, shuffle(tmp1, tmp1, [2,3,0,1,6,7,4,5]))
	# 0 + 1		1 + 0	2 + 3	3 + 2	4 + 5	5 + 4	6 + 7	7 + 6
	tmp3 = add(tmp2, shuffle(tmp2, tmp2, [1,0,3,2,5,4,7,6]))
	return tmp3[0]

def xor(a, b):
	result = [0] * len(a)
	for i in range(len(a)):
		result[i] = a[i] ^ b[i]
	return result

def shuffle(a, b, mask):
	result = [0] * len(a)
	for i in range(len(a)):
		if mask[i] >= len(a):
			result[i] = b[mask[i] - len(a)]
		else:
			result[i] = a[mask[i]]
	return result
	
#1,2,3,4,5,6,7,8

#inserting 4.5
#expected result: 1,2,3,4,4.5,5,6,7

#cmp(input, replicate(4.5)) = [0,0,0,0,-1,-1,-1,-1]

#select(input, replicate(4.5), cmp mask) = [1,2,3,4,4.5,4.5,4.5,4.5]
#cmp mask + 1 = [1,1,1,1,0,0,0,0]
#sum(cmp mask + 1) = 4
#cmp mask[sum] = 0 = [0,0,0,0,0,-1,-1,-1]
#flip prev = [-1,-1,-1,-1,-1,0,0,0]
#select(replicate(9), [9,1,2,3,4,5,6,7], flip mask) = [9,9,9,9,9,5,6,7]
#shuffle([-1,-1,-1,-1,-1,-1,-1,-1], [0,0,0,0,0,0,0,0], prev mask) = [0,0,0,0,0,-1,-1,-1]
#select(result, input, prev mask) = [1,2,3,4,4.5,5,6,7]

def sorted_gt_insert(a, b, val_arr, val):
	width = len(a)
	wide_b = replicate(b, width)
	wide_val = replicate(val, width)
	
	rotate_right_mask = [8,0,1,2,3,4,5,6]

	tmp1 = gt_mask(a, wide_b)
	#print tmp1
	tmp2 = select(a, wide_b, tmp1)
	tmp20 = select(val_arr, wide_val, tmp1)
	#print tmp2
	tmp1 = shuffle(tmp1, replicate(0, width), rotate_right_mask)
	#print tmp1
	tmp5 = xor(tmp1, replicate(-1, width))
	#print tmp5
	tmp6 = select(rotate_right_mask, [8,9,10,11,12,13,14,15], tmp5)
	#print tmp6
	tmp7 = shuffle(a, tmp2, tmp6)
	tmp70 = shuffle(val_arr, tmp20, tmp6)
	print 'Sorted key insert: {0} <- {1} = {2}'.format(a, b, tmp7)
	#print 'Sorted value insert: {0} <- {1} = {2}'.format(val_arr, val, tmp70)
	return (tmp7, tmp70)

def sorted_lt_insert(a, b):
	width = len(a)
	wide_b = replicate(b, width)

	tmp1 = lt_mask(a, wide_b)
	#print tmp1
	tmp2 = select(a, wide_b, tmp1)
	#print tmp2
	tmp3 = add(tmp1, replicate(1, width))
	#print tmp3
	tmp4 = hsum(tmp3)
	#print tmp4
	if tmp4 < width:
		tmp1[tmp4] = 0
	#print tmp1
	tmp5 = xor(tmp1, replicate(-1, width))
	#print tmp5
	tmp6 = select([8,0,1,2,3,4,5,6], [8,9,10,11,12,13,14,15], tmp5)
	#print tmp6
	tmp7 = shuffle(a, tmp2, tmp6)
	print 'Sorted insert: {0} <- {1} = {2}'.format(a, b, tmp7)
	return tmp7

sorted_gt_insert([1,2,3,4,5,6,7,8], 4.5, [0] * 8, 0)
sorted_gt_insert([1,2,3,4,5,6,7,8], 7.5, [0] * 8, 0)
sorted_gt_insert([1,2,3,4,5,6,7,8], 9.5, [0] * 8, 0)
sorted_gt_insert([1,2,3,4,5,6,7,8], 1.5, [0] * 8, 0)
sorted_gt_insert([1,2,3,4,5,6,7,8], 0.5, [0] * 8, 0)
sorted_gt_insert([1,2,3,4,5,6,7,8], 5, [0] * 8, 0)

#print ''

#foo = [100000] * 8
#val = [0] * 8
#foo, val = sorted_gt_insert(foo, 0, val, 3912)
#foo, val = sorted_gt_insert(foo, 0, val, 12032)
#foo, val = sorted_gt_insert(foo, 1, val, 28521)
#foo, val = sorted_gt_insert(foo, 1, val, 36632)
#foo, val = sorted_gt_insert(foo, 0, val, 18248)
#foo, val = sorted_gt_insert(foo, 0, val, 42848)

print ''

sorted_lt_insert([8,7,6,5,4,3,2,1], 4.5)
sorted_lt_insert([8,7,6,5,4,3,2,1], 7.5)
sorted_lt_insert([8,7,6,5,4,3,2,1], 9.5)
sorted_lt_insert([8,7,6,5,4,3,2,1], 1.5)
sorted_lt_insert([8,7,6,5,4,3,2,1], 0.5)





