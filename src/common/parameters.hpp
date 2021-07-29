#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP

struct ParameterInfo {
	float min;
	float max;
	float dflt;
	bool integer;

	float range() const noexcept { return max - min; }
};

static constexpr ParameterInfo parameter_infos[] = {
	{0,0,0, false}, // Atom Ports
	{0,0,0, false}, // 1

	{-1,1,0, false}, // Audio
	{-1,1,0, false},
	{-1,1,0, false},
	{-1,1,0, false}, // 5

	{0,100,100, false}, // Mixer
	{0,100,80, false},
	{0,100,20, false},
	{0,100,10, false},
	{0,100,20, false}, // 10

	{0,1,1, true}, // Predelay/Interpolation
	{0,100,100, false},
	{0,400,20, false}, // 13

	{0,1,0, true}, // Early
	{15,22000,15, false},
	{0,1,0, true},
	{15,22000,20000, false},
	{1,50,12, true},
	{0,500,200, false},
	{0,100,100, false},
	{0,1,0.5f, false},
	{0,8,7, true},
	{10,100,20, false},
	{0,3,0, false},
	{0,5,1, false},
	{0,1,0.7f, false}, // 26

	{0,1,0, true}, // Late
	{1,12,3, true},
	{0.05f,1000,100, false},
	{0,50,0.2f, false},
	{0,5,0.2f, false},
	{0,1,0.7f, false},
	{0,8,7, true},
	{10,100,50, false},
	{0,3,0.2f, false},
	{0,5,0.5f, false},
	{0,1,0.7f, false},
	{0,1,0, true},
	{15,22000,100, false},
	{-24,0,-2, false},
	{0,1,0, true},
	{15,22000,1500, false},
	{-24,0,-3, false},
	{0,1,0, true},
	{15,22000,20000, false}, // 45

	{0,100,80, false}, // seeds
	{1,99999,1, true},
	{1,99999,1, true},
	{1,99999,1, true},
	{1,99999,1, true}, // 50

	{-12,12,-12, false}, // Distortion
	{-12,12,-12, false} // 52
};

#endif
