//
// Copyright (c) 2008-2016 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/AnimatedModel.h>
#include <Urho3D/Graphics/AnimationController.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Input/Controls.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Physics/CollisionShape.h>
#include <Urho3D/Physics/PhysicsWorld.h>
#include <Urho3D/Physics/RigidBody.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>

#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>

#include<Urho3D/UI/LineEdit.h>
#include<Urho3D/UI/Button.h>
#include<Urho3D/UI/UIEvents.h>
#include<Urho3D/UI/Window.h>
#include<Urho3D/UI/CheckBox.h>
#include <Urho3D/Graphics/Skybox.h>

#include<Urho3D/Physics/PhysicsEvents.h>

#include "Character.h"
#include "MainGame.h"
#include "Touch.h"
#include "BoidSet.h"
#include "Bullet.h"

#include <Urho3D/DebugNew.h>

static const StringHash E_CUSTOMEVENT("CustomEvent");

// Custom remote event we use to tell the client which object they controlstatic
const StringHash E_CLIENTOBJECTAUTHORITY("ClientObjectAuthority");
// Identifier for the node ID parameter in the event datastatic 
const StringHash PLAYER_ID("IDENTITY");
// Custom event on server, client has pressed button that it wants to start game
static const StringHash E_CLIENTISREADY("ClientReadyToStart");

URHO3D_DEFINE_APPLICATION_MAIN(MainGame)

const int numOfBoidSet = 4;
BoidSet* boidSet[4];
Bullet* bullets;

int FrameSkipper = 0;

// Which port this is running on 
static const unsigned short SERVER_PORT = 5845;

MainGame::MainGame(Context* context) :
	Sample(context),
	firstPerson_(false)
{
	//TUTORIAL: TODO
	Character::RegisterObject(context);
}

MainGame::~MainGame()
{
}

void MainGame::Start()
{
	// Execute base class startup
	Sample::Start();
	if (touchEnabled_)
		touch_ = new Touch(context_, TOUCH_SENSITIVITY);


	for (int i = 0; i < numOfBoidSet; ++i)
	{
		boidSet[i] = new BoidSet();
	}

	// Subscribe to necessary events
	SubscribeToEvents();

	// Set the mouse mode to use in the sample
	Sample::InitMouseMode(MM_RELATIVE);

	//create main menu for users to select host or join
	CreateMainMenu();
	menuVisable = true;

}


void MainGame::CreateCharacter()
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();

	Node* objectNode = scene_->CreateChild("Jack");
	objectNode->SetPosition(Vector3(0.0f, 1.0f, 0.0f));

	// Create the rendering component + animation controller
	AnimatedModel* object = objectNode->CreateComponent<AnimatedModel>();
	object->SetModel(cache->GetResource<Model>("Models/Jack.mdl"));
	object->SetMaterial(cache->GetResource<Material>("Materials/Jack.xml"));
	object->SetCastShadows(true);
	objectNode->CreateComponent<AnimationController>();

	// Set the head bone for manual control
	object->GetSkeleton().GetBone("Bip01_Head")->animated_ = false;

	// Create rigidbody, and set non-zero mass so that the body becomes dynamic
	RigidBody* body = objectNode->CreateComponent<RigidBody>();
	body->SetCollisionLayer(1);
	body->SetMass(1.0f);

	// Set zero angular factor so that physics doesn't turn the character on its own.
	// Instead we will control the character yaw manually
	body->SetAngularFactor(Vector3::ZERO);

	// Set the rigidbody to signal collision also when in rest, so that we get ground collisions properly
	body->SetCollisionEventMode(COLLISION_ALWAYS);

	// Set a capsule shape for collision
	CollisionShape* shape = objectNode->CreateComponent<CollisionShape>();
	shape->SetCapsule(0.7f, 1.8f, Vector3(0.0f, 0.9f, 0.0f));

	// Create the character logic component, which takes care of steering the rigidbody
	// Remember it so that we can set the controls. Use a WeakPtr because the scene hierarchy already owns it
	// and keeps it alive as long as it's not removed from the hierarchy
	character_ = objectNode->CreateComponent<Character>();
}

void MainGame::CreateInstructions()
{

}

void MainGame::SubscribeToEvents()
{
	printf("SubToEvents");
	// Subscribe to Update event for setting the character controls before physics simulation
	SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(MainGame, HandleUpdate));


	// Subscribe to PostUpdate event for updating the camera position after physics simulation
	SubscribeToEvent(E_POSTUPDATE, URHO3D_HANDLER(MainGame, HandlePostUpdate));

	SubscribeToEvent(E_PHYSICSPRESTEP, URHO3D_HANDLER(MainGame, HandlePhysicsPre));

	SubscribeToEvent(E_CLIENTCONNECTED, URHO3D_HANDLER(MainGame, ServerConnect));

	SubscribeToEvent(E_CLIENTDISCONNECTED, URHO3D_HANDLER(MainGame, HandleDisconnect));

	SubscribeToEvent(E_CLIENTSCENELOADED, URHO3D_HANDLER(MainGame, HandleClientFinishedLoading));

	SubscribeToEvent(E_CUSTOMEVENT, URHO3D_HANDLER(MainGame, HandleCustomEvent));
	GetSubsystem<Network>()->RegisterRemoteEvent(E_CUSTOMEVENT);

	SubscribeToEvent(E_CLIENTISREADY, URHO3D_HANDLER(MainGame, HandleClientToServerReady));
	GetSubsystem<Network>()->RegisterRemoteEvent(E_CLIENTISREADY);

	SubscribeToEvent(E_CLIENTOBJECTAUTHORITY, URHO3D_HANDLER(MainGame, HandleServerToClientObjects));
	GetSubsystem<Network>()->RegisterRemoteEvent(E_CLIENTOBJECTAUTHORITY);

	//SubscribeToEvent(E_KEYDOWN, URHO3D_HANDLER(Sample, HandleKeyDown));

	// Unsubscribe the SceneUpdate event from base class as the camera node is being controlled in HandlePostUpdate() in this sample
	UnsubscribeFromEvent(E_SCENEUPDATE);
}

void MainGame::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
	using namespace Update;

	Input* input = GetSubsystem<Input>();

	float time = eventData[P_TIMESTEP].GetFloat();

	if (character_)
	{
		// Clear previous controls
		character_->controls_.Set(CTRL_FORWARD | CTRL_BACK | CTRL_LEFT | CTRL_RIGHT | CTRL_JUMP, false);

		// Update controls using touch utility class
		if (touch_)
			touch_->UpdateTouches(character_->controls_);

		// Update controls using keys
		UI* ui = GetSubsystem<UI>();
		if (!ui->GetFocusElement())
		{
			if (!touch_ || !touch_->useGyroscope_)
			{
				character_->controls_.Set(CTRL_FORWARD, input->GetKeyDown(KEY_W));
				character_->controls_.Set(CTRL_BACK, input->GetKeyDown(KEY_S));
				character_->controls_.Set(CTRL_LEFT, input->GetKeyDown(KEY_A));
				character_->controls_.Set(CTRL_RIGHT, input->GetKeyDown(KEY_D));
			}
			character_->controls_.Set(CTRL_JUMP, input->GetKeyDown(KEY_SPACE));

			// Add character yaw & pitch from the mouse motion or touch input
			if (touchEnabled_)
			{
				for (unsigned i = 0; i < input->GetNumTouches(); ++i)
				{
					TouchState* state = input->GetTouch(i);
					if (!state->touchedElement_)    // Touch on empty space
					{
						Camera* camera = cameraNode_->GetComponent<Camera>();
						if (!camera)
							return;

						Graphics* graphics = GetSubsystem<Graphics>();
						character_->controls_.yaw_ += TOUCH_SENSITIVITY * camera->GetFov() / graphics->GetHeight() * state->delta_.x_;
						character_->controls_.pitch_ += TOUCH_SENSITIVITY * camera->GetFov() / graphics->GetHeight() * state->delta_.y_;
					}
				}
			}
			else
			{
				if (!ui->GetCursor()->IsVisible()) {
					character_->controls_.yaw_ += (float)input->GetMouseMoveX() * YAW_SENSITIVITY;
					character_->controls_.pitch_ += (float)input->GetMouseMoveY() * YAW_SENSITIVITY;
				}
			}
			// Limit pitch
			character_->controls_.pitch_ = Clamp(character_->controls_.pitch_, -80.0f, 80.0f);
			// Set rotation already here so that it's updated every rendering frame instead of every physics frame
			character_->GetNode()->SetRotation(Quaternion(character_->controls_.yaw_, Vector3::UP));

			// Switch between 1st and 3rd person
			if (input->GetKeyPress(KEY_F))
				firstPerson_ = !firstPerson_;

			// Turn on/off gyroscope on mobile platform
			if (touch_ && input->GetKeyPress(KEY_G))
				touch_->useGyroscope_ = !touch_->useGyroscope_;

			// Check for loading / saving the scene
			if (input->GetKeyPress(KEY_F5))
			{
				File saveFile(context_, GetSubsystem<FileSystem>()->GetProgramDir() + "Data/Scenes/CharacterDemo.xml", FILE_WRITE);
				scene_->SaveXML(saveFile);
			}
			if (input->GetKeyPress(KEY_F7))
			{
				File loadFile(context_, GetSubsystem<FileSystem>()->GetProgramDir() + "Data/Scenes/CharacterDemo.xml", FILE_READ);
				scene_->LoadXML(loadFile);
				// After loading we have to reacquire the weak pointer to the Character component, as it has been recreated
				// Simply find the character's scene node by name as there's only one of them
				Node* characterNode = scene_->GetChild("Jack", true);
				if (characterNode)
					character_ = characterNode->GetComponent<Character>();
			}

		}
	}

	if (input->GetKeyPress(KEY_M)) {
		printf("M pressed");
		menuVisable = !menuVisable;
	}

	//float calctime = 0;
	for (int i = 0; i < numOfBoidSet; i++)
	{
		if (i % 4 == FrameSkipper % 4);
		if (boidSet[i]->isActive)
			boidSet[i]->Update(time);
		//calctime += time;

	}
	FrameSkipper++;

	//printf("Time to calc is %f\n", calctime);


	if (bullets != nullptr)
	{
		bullets->Move();

		if (bullets->pRigidBody != nullptr) {
			Vector3 length = bullets->pRigidBody->GetPosition();
			float d = length.Length();
			if (d > 60.0f)
			{
				bullets->GetNode()->Remove();
				bullets->~Bullet();
				bullets = nullptr;
			}
		}
	}
}

void MainGame::SubscribeToCollisions(Node* node)
{
	if (node)
		SubscribeToEvent(node, E_NODECOLLISION, URHO3D_HANDLER(MainGame, HandleCollisions));
}

void MainGame::HandlePostUpdate(StringHash eventType, VariantMap& eventData)
{
	UI* ui = GetSubsystem<UI>();
	Input* input = GetSubsystem<Input>();
	//printf(menuVisable ? "true\n" : "false\n");
	ui->GetCursor()->SetVisible(menuVisable);
	window->SetVisible(menuVisable);

	// Only move the camera if we have a controllable object 
	if (clientObject)
	{
		Node* ballNode = this->scene_->GetNode(clientObject);
		if (ballNode)
		{
			yaw += (float)input->GetMouseMoveX() * YAW_SENSITIVITY;
			pitch += (float)input->GetMouseMoveY() * YAW_SENSITIVITY;

			cameraNode_->SetRotation(Quaternion(pitch, yaw, 0));
			const float CAMERA_DISTANCE = 10.0f;
			cameraNode_->SetPosition(ballNode->GetPosition() + cameraNode_->GetRotation() * Vector3::BACK * CAMERA_DISTANCE + (Vector3::UP * CAMERA_DISTANCE * .25f));
		}
	}

	if (!character_)
		return;

	Node* characterNode = character_->GetNode();

	// Get camera lookat dir from character yaw + pitch
	Quaternion rot = characterNode->GetRotation();
	Quaternion dir = rot * Quaternion(character_->controls_.pitch_, Vector3::RIGHT);

	// Turn head to camera pitch, but limit to avoid unnatural animation
	Node* headNode = characterNode->GetChild("Bip01_Head", true);
	float limitPitch = Clamp(character_->controls_.pitch_, -45.0f, 45.0f);
	Quaternion headDir = rot * Quaternion(limitPitch, Vector3(1.0f, 0.0f, 0.0f));
	// This could be expanded to look at an arbitrary target, now just look at a point in front
	Vector3 headWorldTarget = headNode->GetWorldPosition() + headDir * Vector3(0.0f, 0.0f, 1.0f);
	headNode->LookAt(headWorldTarget, Vector3(0.0f, 1.0f, 0.0f));
	// Correct head orientation because LookAt assumes Z = forward, but the bone has been authored differently (Y = forward)
	headNode->Rotate(Quaternion(0.0f, 90.0f, 90.0f));

	if (firstPerson_)
	{
		cameraNode_->SetPosition(headNode->GetWorldPosition() + rot * Vector3(0.0f, 0.15f, 0.2f));
		cameraNode_->SetRotation(dir);
	}
	else
	{
		// Third person camera: position behind the character
		Vector3 aimPoint = characterNode->GetPosition() + rot * Vector3(0.0f, 1.7f, 0.0f);

		// Collide camera ray with static physics objects (layer bitmask 2) to ensure we see the character properly
		Vector3 rayDir = dir * Vector3::BACK;
		float rayDistance = touch_ ? touch_->cameraDistance_ : CAMERA_INITIAL_DIST;
		PhysicsRaycastResult result;
		scene_->GetComponent<PhysicsWorld>()->RaycastSingle(result, Ray(aimPoint, rayDir), rayDistance, 2);
		if (result.body_)
			rayDistance = Min(rayDistance, result.distance_);
		rayDistance = Clamp(rayDistance, CAMERA_MIN_DIST, CAMERA_MAX_DIST);

		cameraNode_->SetPosition(aimPoint + rayDir * rayDistance);
		cameraNode_->SetRotation(dir);
	}

}

void MainGame::CreateClientScene()
{
	if (scene_)
		scene_->Clear();
	printf("client scene created \n");
	ResourceCache* cache = GetSubsystem<ResourceCache>();

	scene_ = new Scene(context_);
	// Create scene subsystem components
	scene_->CreateComponent<Octree>(LOCAL);
	scene_->CreateComponent<PhysicsWorld>(LOCAL);

	// Create camera and define viewport. We will be doing load / save, so it's convenient to create the camera outside the scene,
	// so that it won't be destroyed and recreated, and we don't have to redefine the viewport on load
	cameraNode_ = new Node(context_);
	Camera* camera = cameraNode_->CreateComponent<Camera>(LOCAL);
	cameraNode_->SetPosition(Vector3(-5.0f, 5.0f, 0.0f));
	camera->SetFarClip(300.0f);
	GetSubsystem<Renderer>()->SetViewport(0, new Viewport(context_, scene_, camera));

	// Create static scene content. First create a zone for ambient lighting and fog control
	//Node* zoneNode = scene_->CreateChild("Zone");
	//Zone* zone = zoneNode->CreateComponent<Zone>(LOCAL);
	//zone->SetAmbientColor(Color(0.15f, 0.15f, 0.15f));
	//zone->SetFogColor(Color(0.5f, 0.5f, 0.7f));
	//zone->SetFogStart(100.0f);
	//zone->SetFogEnd(300.0f);
	//zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));

	// Create a directional light with cascaded shadow mapping
	Node* lightNode = scene_->CreateChild("DirectionalLight", LOCAL);
	lightNode->SetDirection(Vector3(0.3f, -0.5f, 0.425f));
	Light* light = lightNode->CreateComponent<Light>();
	light->SetLightType(LIGHT_DIRECTIONAL);
	light->SetCastShadows(true);
	light->SetShadowBias(BiasParameters(0.00025f, 0.5f));
	light->SetShadowCascade(CascadeParameters(10.0f, 50.0f, 200.0f, 0.0f, 0.8f));
	light->SetSpecularIntensity(0.5f);

	// Create the floor object
	Node* floorNode = scene_->CreateChild("Floor", LOCAL);
	floorNode->SetPosition(Vector3(0.0f, -0.5f, 0.0f));
	floorNode->SetScale(Vector3(200.0f, 1.0f, 200.0f));
	StaticModel* object = floorNode->CreateComponent<StaticModel>(LOCAL);
	object->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
	object->SetMaterial(cache->GetResource<Material>("Materials/Stone.xml"));
	

	RigidBody* body = floorNode->CreateComponent<RigidBody>(LOCAL);
	// Use collision layer bit 2 to mark world scenery. This is what we will raycast against to prevent camera from going
	// inside geometry
	body->SetCollisionLayer(2);
	CollisionShape* shape = floorNode->CreateComponent<CollisionShape>(LOCAL);
	shape->SetBox(Vector3::ONE);

}


void MainGame::CreateServerScene()
{
	printf("Server created \n");
	ResourceCache* cache = GetSubsystem<ResourceCache>();

	scene_ = new Scene(context_);
	// Create scene subsystem components
	scene_->CreateComponent<Octree>(LOCAL);
	scene_->CreateComponent<PhysicsWorld>(LOCAL);

	Node* skyNode = scene_->CreateChild("Sky");
	Skybox* skybox = skyNode->CreateComponent<Skybox>();
	skybox->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
	skybox->SetMaterial(cache->GetResource<Material>("Materials/Skybox.xml"));

	// Create camera and define viewport. We will be doing load / save, so it's convenient to create the camera outside the scene,
	// so that it won't be destroyed and recreated, and we don't have to redefine the viewport on load
	cameraNode_ = new Node(context_);
	Camera* camera = cameraNode_->CreateComponent<Camera>(LOCAL);
	cameraNode_->SetPosition(Vector3(0.0f, 5.0f, 0.0f));
	camera->SetFarClip(300.0f);
	GetSubsystem<Renderer>()->SetViewport(0, new Viewport(context_, scene_, camera));

	// Create static scene content. First create a zone for ambient lighting and fog control
	Node* zoneNode = scene_->CreateChild("Zone");
	Zone* zone = zoneNode->CreateComponent<Zone>();
	zone->SetAmbientColor(Color(0.15f, 0.15f, 0.15f));
	zone->SetFogColor(Color(0.5f, 0.5f, 0.7f));
	zone->SetFogStart(100.0f);
	zone->SetFogEnd(300.0f);
	zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));

	// Create a directional light with cascaded shadow mapping
	Node* lightNode = scene_->CreateChild("DirectionalLight", LOCAL);
	lightNode->SetDirection(Vector3(0.3f, -0.5f, 0.425f));
	Light* light = lightNode->CreateComponent<Light>(LOCAL);
	light->SetLightType(LIGHT_DIRECTIONAL);
	light->SetCastShadows(true);
	light->SetShadowBias(BiasParameters(0.00025f, 0.5f));
	light->SetShadowCascade(CascadeParameters(10.0f, 50.0f, 200.0f, 0.0f, 0.8f));
	light->SetSpecularIntensity(0.5f);

	// Create the floor object
	Node* floorNode = scene_->CreateChild("Floor", LOCAL);
	floorNode->SetPosition(Vector3(0.0f, -0.5f, 0.0f));
	floorNode->SetScale(Vector3(200.0f, 1.0f, 200.0f));
	StaticModel* object = floorNode->CreateComponent<StaticModel>();
	object->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
	object->SetMaterial(cache->GetResource<Material>("Materials/Stone.xml"));

	RigidBody* body = floorNode->CreateComponent<RigidBody>();
	// Use collision layer bit 2 to mark world scenery. This is what we will raycast against to prevent camera from going
	// inside geometry
	body->SetCollisionLayer(2);
	CollisionShape* shape = floorNode->CreateComponent<CollisionShape>();
	shape->SetBox(Vector3::ONE);

	//create Arena
	Node* AreanNode = scene_->CreateChild("Arena");
	AreanNode->SetPosition(Vector3(0, 0.0f, 0));
	AreanNode->SetRotation(Quaternion(0.0f, 0, 0.0f));
	StaticModel* Areanobject = AreanNode->CreateComponent<StaticModel>();
	Areanobject->SetModel(cache->GetResource<Model>("Models/Arena.mdl"));
	Areanobject->SetMaterial(cache->GetResource<Material>("Materials/Mushroom.xml"));
	Areanobject->SetCastShadows(true);

	RigidBody* Areanbody = AreanNode->CreateComponent<RigidBody>();
	Areanbody->SetCollisionLayer(2);
	CollisionShape* Areanshape = AreanNode->CreateComponent<CollisionShape>();
	Areanshape->SetTriangleMesh(Areanobject->GetModel(), 0);

	//create tge biuds
	for (int i = 0; i < numOfBoidSet; i++)
	{
		boidSet[i]->Initialise(cache, scene_);
		boidSet[i]->isActive = true;
	}

}

Controls MainGame::FromClientToServer()
{
	return Controls();
}


void MainGame::CreateMainMenu()
{
	//OpenConsoleWindow();
	InitMouseMode(MM_RELATIVE);

	ResourceCache* cache = GetSubsystem<ResourceCache>();
	UI* ui = GetSubsystem<UI>();
	UIElement* root = ui->GetRoot();
	XMLFile* uiStyle = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
	root->SetDefaultStyle(uiStyle);

	SharedPtr<Cursor> cursor(new Cursor(context_));
	cursor->SetStyleAuto(uiStyle);
	ui->SetCursor(cursor);

	window = new Window(context_);
	root->AddChild(window);

	window->SetMinWidth(384);
	window->SetLayout(LM_VERTICAL, 6, IntRect(6, 6, 6, 6));
	window->SetAlignment(HA_CENTER, VA_CENTER);
	window->SetName("Window");
	window->SetStyleAuto();

	//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

	//close menu button
	Button* Close = CreateButton("", 24, window);
	Close->SetStyle("CloseButton");
	Close->SetAlignment(HA_RIGHT, VA_CENTER);

	Text* text = window->CreateChild<Text>();
	Font* font = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");
	text->SetFont(font, 50);
	text->SetAlignment(HA_CENTER, VA_CENTER);
	text->SetText("Shoot'em Dino");
	window->AddChild(text);

	//CheckBox* c = CreateCheckBox("lol", 24, window);

	Button* connectBtn = CreateButton("Connect", 24, window);
	serverAddress = CreateLineEdit("", 24, window);
	Button* disconectBtn = CreateButton("Disconnect", 24, window);
	Button* startServerBtn = CreateButton("Start Server", 24, window);
	Button* startGame = CreateButton("Client : Start Game", 24, window);
	Button* quitBtn = CreateButton("Quit", 24, window);

	SubscribeToEvent(quitBtn, E_RELEASED, URHO3D_HANDLER(MainGame, HandleQuit));
	SubscribeToEvent(Close, E_RELEASED, URHO3D_HANDLER(MainGame, CloseMenu));
	SubscribeToEvent(connectBtn, E_RELEASED, URHO3D_HANDLER(MainGame, HandleConnect));
	SubscribeToEvent(disconectBtn, E_RELEASED, URHO3D_HANDLER(MainGame, HandleDisconnect));
	SubscribeToEvent(startServerBtn, E_RELEASED, URHO3D_HANDLER(MainGame, HandleStartServer));
	SubscribeToEvent(startGame, E_RELEASED, URHO3D_HANDLER(MainGame, HandleClientStartGame));
}

Button* MainGame::CreateButton(const String& text, int pHeight, Urho3D::Window* whichWindow)
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();
	Font* font = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");
	Button* button = whichWindow->CreateChild<Button>();
	button->SetMinHeight(pHeight);
	button->SetStyleAuto();
	Text* buttonText = button->CreateChild<Text>();
	buttonText->SetFont(font, 12);
	buttonText->SetAlignment(HA_CENTER, VA_CENTER);
	buttonText->SetText(text);
	whichWindow->AddChild(button);
	return button;
}

LineEdit* MainGame::CreateLineEdit(const String& text, int pHeight, Urho3D::Window* whichWindow)
{
	LineEdit* lineEdit = whichWindow->CreateChild<LineEdit>();
	lineEdit->SetMinHeight(pHeight);
	lineEdit->SetAlignment(HA_CENTER, VA_CENTER);
	lineEdit->SetText(text);
	whichWindow->AddChild(lineEdit);
	lineEdit->SetStyleAuto();

	return lineEdit;
}

CheckBox* MainGame::CreateCheckBox(const String& text, int pHeight, Urho3D::Window* whichWindow)
{
	CheckBox* checkbox = whichWindow->CreateChild<CheckBox>();
	checkbox->SetMinHeight(pHeight);
	checkbox->SetAlignment(HA_CENTER, VA_CENTER);
	checkbox->SetStyleAuto();

	whichWindow->AddChild(checkbox);

	return  checkbox;
}

void MainGame::HandleQuit(StringHash eventType, VariantMap& eventData)
{
	engine_->Exit();
}

void MainGame::CloseMenu(StringHash eventType, VariantMap& eventData)
{
	printf("Menu Close");
	menuVisable = !menuVisable;
}

void MainGame::HandleConnect(StringHash eventType, VariantMap& eventData)
{
	CreateClientScene();

	Network* network = GetSubsystem<Network>();

	String address = serverAddress->GetText().Trimmed();

	if (address.Empty())
		address = "localhost";

	network->Connect(address, SERVER_PORT, scene_);

	//VariantMap remoteData;
	//remoteData["aValueRemoteValue"] = 0;
	//network->BroadcastRemoteEvent(E_CUSTOMEVENT, true, remoteData);

}

void MainGame::ServerConnect(StringHash eventType, VariantMap& eventData)
{

	printf("Serverside: Client has connected ! \n");

	using namespace ClientConnected;


	Connection* newConnection = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

	newConnection->SetScene(scene_);

	VariantMap remoteData;
	remoteData["aValueRemoteValue"] = 0;
	newConnection->SendRemoteEvent(E_CUSTOMEVENT, true, remoteData);
}

void MainGame::HandleDisconnect(StringHash eventType, VariantMap& eventData)
{
	Log::WriteRaw("HandleDisconnect has been pressed. \n");

	Network* network = GetSubsystem<Network>();

	Connection* serverConnection = network->GetServerConnection();

	// Running as Client    
	if (serverConnection)
	{
		serverConnection->Disconnect();

		scene_->Clear();

		clientObject = 0;
	}
	// Running as a server, stop it    
	else if (network->IsServerRunning())
	{
		network->StopServer();

		for (int i = 0; i < numOfBoidSet; i++)
		{
			boidSet[i]->isActive = false;
		}

		scene_->Clear();
	}
}

void MainGame::HandleStartServer(StringHash eventType, VariantMap& eventData)
{
	CreateServerScene();

	Network* network = GetSubsystem<Network>();
	network->StartServer(SERVER_PORT);
	menuVisable = !menuVisable;

}

Controls MainGame::ClientToSeverControls()
{
	Input* input = GetSubsystem<Input>();

	Controls controls;

	//Check which button has been pressed, keep track
	controls.Set(CTRL_FORWARD, input->GetKeyDown(KEY_W));
	controls.Set(CTRL_BACK, input->GetKeyDown(KEY_S));
	controls.Set(CTRL_LEFT, input->GetKeyDown(KEY_A));
	controls.Set(CTRL_RIGHT, input->GetKeyDown(KEY_D));

	controls.Set(1024, input->GetKeyDown(KEY_E));
	// mouse yaw to server
	controls.yaw_ = yaw;
	controls.pitch_ = pitch;

	return controls;
}

void MainGame::ProcessControls()
{
	Network* network = GetSubsystem<Network>();

	const Vector<SharedPtr<Connection> >& connections = network->GetClientConnections();

	for (unsigned i = 0; i < connections.Size(); ++i) {
		Connection* connection = connections[i];
		Node* ballNode = serverObjects[connection];

		if (!ballNode)
			continue;

		RigidBody* body = ballNode->GetComponent<RigidBody>();

		const Controls& controls = connection->GetControls();

		Quaternion rotation(0, controls.yaw_, 0);

		const float moveTorque = .1f;

		if (controls.buttons_ & CTRL_FORWARD)
			printf("Received from Client: Controls buttons FORWARD \n");
		if (controls.buttons_ & CTRL_BACK)
			printf("Received from Client: Controls buttons BACK \n");
		if (controls.buttons_ & CTRL_LEFT)
			printf("Received from Client: Controls buttons LEFT \n");
		if (controls.buttons_ & CTRL_RIGHT)
			printf("Received from Client: Controls buttons RIGHT \n");

		if (controls.buttons_ & CTRL_FORWARD)
			body->SetPosition(body->GetPosition() + rotation * Vector3::FORWARD * moveTorque);
		if (controls.buttons_ & CTRL_BACK)
			body->SetPosition(body->GetPosition() + rotation * Vector3::BACK * moveTorque);
		if (controls.buttons_ & CTRL_RIGHT)
			body->SetPosition(body->GetPosition() + rotation * Vector3::RIGHT * moveTorque);
		if (controls.buttons_ & CTRL_LEFT)
			body->SetPosition(body->GetPosition() + rotation * Vector3::LEFT * moveTorque);
		body->SetRotation(rotation);

		if (controls.buttons_ & 1024)
		{
			printf("Creating a bullet\n");

			if (bullets != nullptr)
				break;

			Bullet* b = new Bullet(Quaternion(0, controls.yaw_, 0));

			ResourceCache* cache = GetSubsystem<ResourceCache>();
			b->Initialize(cache, scene_, body->GetPosition()+ Vector3(0,1,0));
			if (bullets == nullptr) {
				bullets = b;
				SubscribeToCollisions(b->GetNode());
			}
		}
	}
}


void MainGame::HandlePhysicsPre(StringHash eventType, VariantMap& eventData)
{
	Network* network = GetSubsystem<Network>();
	Connection* serverConnection = network->GetServerConnection();

	if (serverConnection) {
		serverConnection->SetPosition(cameraNode_->GetPosition());
		serverConnection->SetControls(ClientToSeverControls());

		VariantMap remoteData;
		remoteData["aValueRemoteValue"] = 0;
		serverConnection->SendRemoteEvent(E_CUSTOMEVENT, true, remoteData);
	}
	else if (network->IsServerRunning()) {
		ProcessControls();
	}
}

void MainGame::HandleClientFinishedLoading(StringHash eventType, VariantMap& eventData)
{
	printf("Client Scene Loaded");
}

void MainGame::HandleCustomEvent(StringHash eventType, VariantMap& eventData)
{
	printf("This is a custom event\n\n");
}

void MainGame::HandleCollisions(StringHash eventType, VariantMap& eventData)
{
	using namespace NodeCollision;

	RigidBody* bullet = static_cast<RigidBody*>(eventData[P_BODY].GetPtr());
	RigidBody* bird = static_cast<RigidBody*>(eventData[P_OTHERBODY].GetPtr());

	//printf("%s\n", bird->GetNode()->GetName());

	if (bird->GetNode()->GetName().Contains("Boid"))
	{
		printf("Collision is Occouring\n");

		bird->SetUseGravity(true);
		bird->SetLinearVelocity(Vector3::ZERO);
		bullet->SetPosition(Vector3(100, 100, 100));
	}
}

Node* MainGame::CreateControllableObject()
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();

	Node* ballNode = scene_->CreateChild("AClientBall");
	ballNode->SetPosition(Vector3(0, 5.0f, 0));
	ballNode->SetScale(2.5F);

	StaticModel* ballObject = ballNode->CreateComponent<StaticModel>();
	ballObject->SetModel(cache->GetResource<Model>("Models/Mutant/Mutant.mdl"));
	ballObject->SetMaterial(cache->GetResource<Material>("Models/Mutant/Materials/mutant_M.xml"));
	ballNode->SetScale(.75F * Vector3::ONE);

	Node* cam = ballNode->CreateChild("Camera");
	cam->CreateComponent<Camera>();

	RigidBody* body = ballNode->CreateComponent<RigidBody>();
	body->SetMass(1.0f);
	body->SetFriction(1.0f);
	body->SetLinearDamping(0.25f);
	body->SetAngularDamping(0.25f);

	CollisionShape* shape = ballNode->CreateComponent<CollisionShape>();
	shape->SetSphere(0.5f);

	return ballNode;
}

void MainGame::HandleServerToClientObjects(StringHash eventType, VariantMap& eventData)
{
	clientObject = eventData[PLAYER_ID].GetUInt();
	printf("Client ID : %i \n", clientObject);
}

void MainGame::HandleClientToServerReady(StringHash eventType, VariantMap& eventData)
{
	printf("Event sent by the client and running on server:");

	using namespace ClientConnected;
	Connection* newConnection = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

	Node* newObject = CreateControllableObject();
	serverObjects[newConnection] = newObject;

	VariantMap remoteEventData;
	remoteEventData[PLAYER_ID] = newObject->GetID();
	newConnection->SendRemoteEvent(E_CLIENTOBJECTAUTHORITY, true, remoteEventData);
	menuVisable = false;
}

void MainGame::HandleClientStartGame(StringHash eventType, VariantMap& eventData)
{
	printf("Client has pressed START GAME \n");
	if (clientObject == 0) // Client is still observer  
	{
		Network* network = GetSubsystem<Network>();
		Connection* serverConnection = network->GetServerConnection();
		if (serverConnection)
		{
			VariantMap remoteEventData;
			remoteEventData[PLAYER_ID] = 0;
			serverConnection->SendRemoteEvent(E_CLIENTISREADY, true, remoteEventData);
		}
		menuVisable = false;
	}
}