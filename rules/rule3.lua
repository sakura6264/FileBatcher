title="n(1-m)"
helptext="name is like: \r\n1(1~600)\r\n2(601~1200)"
function settext(n,m)
	str=string.format("%d(%d~%d)",n,(n-1)*m+1,n*m)
	return str
end
