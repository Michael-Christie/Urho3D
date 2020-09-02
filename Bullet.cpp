#include "Bullet.h"

Bullet::Bullet(Quaternion rotation)
{
	direction = rotation.Normalized();
	pNode = nullptr;
	pRigidBody = nullptr;
	pCollisionShape = nullptr;
	pObject = nullptr;
	
	//SubscribeToEvent(E_PHYSICSCOLLISIONSTART, _HANDLER(Bullet, HandleCollisions));
}

Bullet::~Bullet()
{
}

void Bullet::Initialize(ResourceCache* pRes, Scene* pScene, Vector3 position)
{
	pNode = pScene->CreateChild("Bullet");
	pRigidBody = pNode->CreateComponent<RigidBody>();
	pCollisionShape = pNode->CreateComponent<CollisionShape>();
	pCollisionShape->SetSphere(1.0f);
	pObject = pNode->CreateComponent<StaticModel>();

	pObject->SetModel(pRes->GetResource<Model>("Models/Sphere.mdl"));

	pRigidBody->SetMass(1.0f);
	pRigidBody->SetUseGravity(false);
	pRigidBody->SetPosition(position + (direction * Vector3::FORWARD * 5.0f));
	pRigidBody->SetRotation(direction);
}

void Bullet::HandleCollisions(StringHash eventType, VariantMap& eventData)
{
}

void Bullet::Move()
{
	if (pRigidBody != nullptr) {
		Vector3 vel = pRigidBody->GetLinearVelocity();
		pRigidBody->SetLinearVelocity(vel.Normalized() + direction * Vector3::FORWARD * moveSpeed);
	}
}

Node* Bullet::GetNode()
{
	return pNode;
}

