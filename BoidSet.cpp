#include "BoidSet.h"

BoidSet::BoidSet()
{
	isActive = false;
	for (int i = 0; i < num; i++) {
		boidList[i] = Boid();
	}
}

void BoidSet::Initialise(ResourceCache *pRes, Scene *pScene)
{
	for (int i = 0; i < num; i++) {
		boidList[i].Initialise(pRes, pScene);
	}											   
}

void BoidSet::Update(float tm)
{
	for (int i = 0; i < num; i++) {
		boidList[i].ComputeForce(&boidList[0], num);
		boidList[i].Update(tm);
	}
}
