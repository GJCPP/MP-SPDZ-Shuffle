#include "math_gadget.h"

int math_gadget::log2(unsigned int val)
{
	unsigned int ret(0);
	while ((static_cast<unsigned int>(1) << ret) < val) ++ret;
	return ret;
}

std::string math_gadget::itos(int i) {
	if (i == 0) return "0";
	std::string ret = "", tem = "";
	if (i < 0) {
		ret = "-";
		i = -i;
	}

	while (i) {
		tem += char('0' + (i % 10));
		i /= 10;
	}
	for (auto itr = tem.rbegin(); itr != tem.rend(); ++itr) ret += *itr;
	return ret;
}
