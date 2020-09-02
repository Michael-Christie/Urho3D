#pragma once
#include "Boid.h"

class BoidSet
{
public:
	const int num = 25;
	Boid boidList[25];
	BoidSet();
	void Initialise(ResourceCache *pRes, Scene *pScene);
	void Update(float tm);

	bool isActive;
}
;