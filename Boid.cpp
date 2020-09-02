#include "Boid.h"

float Boid::Range_FAttract = 60.0f;
float Boid::Range_FRepel = 40.0f;
float Boid::Range_FAlign = 10.0f;

float Boid::FAttract_Vmax = 5.0f;

float Boid::FAttract_Factor = 8.0f;
float Boid::FRepel_Factor = 8.0f;
float Boid::FAlign_Factor = 4.0f;


Boid::Boid() {
	pNode = nullptr;
	pRigidBody = nullptr;
	pCollisionShape = nullptr;
	pObject = nullptr;
	isDead = false;
};

Boid::~Boid()
{
}

void Boid::Initialise(ResourceCache* pRes, Scene* pScene)
{
	pNode = pScene->CreateChild("Boid");
	pRigidBody = pNode->CreateComponent<RigidBody>();
	pCollisionShape = pNode->CreateComponent<CollisionShape>();
	pCollisionShape->SetSphere(0.5F);
	//pRigidBody->SetTrigger(true);
	pObject = pNode->CreateComponent<StaticModel>();

	pObject->SetModel(pRes->GetResource<Model>("Models/ptewing.mdl"));
	pObject->SetMaterial(pRes->GetResource<Material>("Materials/Stone.xml"));

	pRigidBody->SetMass(1.0f);
	pRigidBody->SetUseGravity(false);
	pRigidBody->SetPosition(Vector3(Random(180.0f) - 90.0f, 1.5f , Random(180.0f) - 90.0f));
}

void Boid::ComputeForce(Boid* pBoidList, int numOfBoids)
{
	if (pRigidBody->GetUseGravity()) {
		isDead = true;
		pRigidBody->SetUseGravity(false);
		pRigidBody->SetPosition(Vector3(0, -100, 0));
	}

	force = Vector3(0, 0, 0);
	if (!isDead) {
		//force += Attraction(pBoidList, numOfBoids);
		//force += Seperation(pBoidList, numOfBoids);
		//force += Allign(pBoidList, numOfBoids);
		force = BoidMasterCal(pBoidList, numOfBoids);
		force += AttractionCeneter(pBoidList, numOfBoids);

		force *= Vector3(1, 0, 1);
	}
}

Vector3 Boid::Attraction(Boid* pBoidList, int numOfBoids)
{
	Vector3 fA = Vector3(0, 0, 0);

	Vector3 CoM; //centre of mass, accumulated total	
	int n = 0;   //count number of neigbours
				 //set the force member variable to zero force =Vector3(0, 0, 0);
				 //Search Neighbourhood
	for (int i = 0; i < numOfBoids; i++)
	{
		//the current boid?
		if (this == &pBoidList[i] || pBoidList[i].isDead)
			continue;
		//sep = vector position of this boid from current oid
		Vector3 sep = pRigidBody->GetPosition() - pBoidList[i].pRigidBody->GetPosition();
		float d = sep.Length();
		//distance of boid 
		if (d < Range_FAttract)
		{
			//with range, so is a neighbour
			CoM += pBoidList[i].pRigidBody->GetPosition();
			n++;
		}
	}
	//Attractive force component
	if (n > 0)
	{
		//find average position = centre of mass
		CoM /= n;
		Vector3 dir = (CoM - pRigidBody->GetPosition()).Normalized();
		Vector3 vDesired = dir * FAttract_Vmax;
		fA += (vDesired - pRigidBody->GetLinearVelocity()) * FAttract_Factor;
	}

	return fA;
}

Vector3 Boid::AttractionCeneter(Boid* pBoidList, int numOfBoids)
{
	Vector3 fA = Vector3(0, 0, 0);

	Vector3 CoM; //centre of mass, accumulated total	
	int n = 0;   //count number of neigbours
				 //set the force member variable to zero force =Vector3(0, 0, 0);
				 //Search Neighbourhood
	for (int i = 0; i < numOfBoids; i++)
	{
		//the current boid?
		if (this == &pBoidList[i] || pBoidList[i].isDead)
			continue;
		//sep = vector position of this boid from current oid
		Vector3 sep = pRigidBody->GetPosition() - Vector3(0, 0, 0);
		float d = sep.Length();
		//distance of boid 
		if (d < 10.0f)
		{
			//with range, so is a neighbour
			CoM += pBoidList[i].pRigidBody->GetPosition();
			n++;
		}
	}
	//Attractive force component
	if (n > 0)
	{
		//find average position = centre of mass
		CoM /= n;
		Vector3 dir = (CoM - pRigidBody->GetPosition()).Normalized();
		Vector3 vDesired = dir * FAttract_Vmax;
		fA += (vDesired - pRigidBody->GetLinearVelocity()) * FAttract_Factor;
	}

	return fA;
}

Vector3 Boid::Seperation(Boid* pBoidList, int numOfBoids)
{
	Vector3 fS = Vector3(0, 0, 0);
	Vector3 diff = Vector3();

	int n = 0;

	for (int i = 0; i < numOfBoids; i++)
	{
		if (this == &pBoidList[i] || pBoidList[i].isDead)
			continue;
		Vector3 dist = pRigidBody->GetPosition() - pBoidList[i].pRigidBody->GetPosition();
		float d = dist.Length();
		if (d < Range_FRepel) {
			diff += dist.Normalized();
			//n++;
		}
	}

	fS = diff * FRepel_Factor;

	return fS;
}

Vector3 Boid::Allign(Boid* pBoidList, int numOfBoids)
{
	Vector3 fA = Vector3(0, 0, 0);
	Vector3 direction = Vector3(0, 0, 0);

	int n = 0;

	for (int i = 0; i < numOfBoids; i++)
	{
		if (this == &pBoidList[i] || pBoidList[i].isDead)
			continue;
		Vector3 dist = pRigidBody->GetPosition() - pBoidList[i].pRigidBody->GetPosition();
		float d = dist.Length();
		if (d < Range_FAlign) {
			direction += pBoidList[i].pRigidBody->GetLinearVelocity();
			n++;
		}
	}

	if (n > 0)
	{
		//find average position = centre of mass
		direction /= n;

		fA += (direction - pRigidBody->GetLinearVelocity()) * FAlign_Factor;
	}

	return fA;
}

Vector3 Boid::BoidMasterCal(Boid* pBoidList, int numOfBoids)
{
	Vector3 fA = Vector3(0, 0, 0);
	Vector3 diff = Vector3();
	Vector3 direction = Vector3();

	Vector3 CoM; //centre of mass, accumulated total	
	int n = 0;   //count number of neigbours
				 //set the force member variable to zero force =Vector3(0, 0, 0);
				 //Search Neighbourhood
	for (int i = 0; i < numOfBoids; i++)
	{
		//the current boid?
		if (this == &pBoidList[i] || pBoidList[i].isDead)
			continue;
		//sep = vector position of this boid from current oid
		Vector3 sep = pRigidBody->GetPosition() - pBoidList[i].pRigidBody->GetPosition();
		float d = sep.Length();
		//distance of boid 
		if (d < Range_FAttract)
		{
			//with range, so is a neighbour
			CoM += pBoidList[i].pRigidBody->GetPosition();
			n++;
		}
		if (d < Range_FRepel)
			diff += sep.Normalized();
		if (d < Range_FAlign) {
			direction += pBoidList[i].pRigidBody->GetLinearVelocity();
		}
	}
	//Attractive force component
	if (n > 0)
	{
		//find average position = centre of mass
		CoM /= n;
		Vector3 dir = (CoM - pRigidBody->GetPosition()).Normalized();
		Vector3 vDesired = dir * FAttract_Vmax;
		fA += (vDesired - pRigidBody->GetLinearVelocity()) * FAttract_Factor;
		direction /= n;
	}

	diff *= FRepel_Factor;
	direction = (direction - pRigidBody->GetLinearVelocity())* FAlign_Factor;

	return fA + diff + direction;
}

void Boid::Update(float tm)
{
	pRigidBody->ApplyForce(force);
	Vector3 vel = pRigidBody->GetLinearVelocity();
	float d = vel.Length();
	if (d < 10.0f)
	{
		d = 10.0f;
		pRigidBody->SetLinearVelocity(vel.Normalized() * d);
	}
	else if (d > 50.0f)
	{
		d = 50.0f;
		pRigidBody->SetLinearVelocity(vel.Normalized() * d);
	}

	if (!isDead) {
		Vector3 vn = vel.Normalized();
		Vector3 cp = -vn.CrossProduct(Vector3(0.0f, 1.0f, 0.0f));
		float dp = cp.DotProduct(vn);
		pRigidBody->SetRotation(Quaternion(Acos(dp), cp));
		Vector3 p = pRigidBody->GetPosition();
		if (p.y_ < 1.4f)
		{
			p.y_ = 1.5f;
			pRigidBody->SetPosition(p);
		}
		else if (p.y_ > 1.6f)
		{
			p.y_ = 1.5f;
			pRigidBody->SetPosition(p);
		}
	}
}
