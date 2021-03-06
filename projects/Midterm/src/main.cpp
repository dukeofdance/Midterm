//Just a simple handler for simple initialization stuffs
#include "Utilities/BackendHandler.h"

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <Texture2D.h>
#include <Texture2DData.h>
#include <MeshBuilder.h>
#include <MeshFactory.h>
#include <NotObjLoader.h>
#include <ObjLoader.h>
#include <VertexTypes.h>
#include <ShaderMaterial.h>
#include <RendererComponent.h>
#include <TextureCubeMap.h>
#include <TextureCubeMapData.h>

#include <Timing.h>
#include <GameObjectTag.h>
#include <InputHelpers.h>

#include <IBehaviour.h>
#include <CameraControlBehaviour.h>
#include <FollowPathBehaviour.h>
#include <SimpleMoveBehaviour.h>

int main() {
	int frameIx = 0;
	float fpsBuffer[128];
	float minFps, maxFps, avgFps;
	int selectedVao = 0; // select cube by default
	std::vector<GameObject> controllables;


	BackendHandler::InitAll();
	
	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(BackendHandler::GlDebugMessage, nullptr);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	// Push another scope so most memory should be freed *before* we exit the app
	{
		#pragma region Shader and ImGui
		Shader::sptr passthroughShader = Shader::Create();
		passthroughShader->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
		passthroughShader->LoadShaderPartFromFile("shaders/passthrough_frag.glsl", GL_FRAGMENT_SHADER);
		passthroughShader->Link();

		// Load our shaders
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured2.glsl", GL_FRAGMENT_SHADER);
		shader->Link();
		//
		glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 10.0f);
		glm::vec3 lightCol = glm::vec3(0.9f, 0.85f, 0.5f);
		float     lightAmbientPow = 0.95f;
		float     lightSpecularPow = 1.0f;
		glm::vec3 ambientCol = glm::vec3(1.0f);
		float     ambientPow = 0.1f;
		float     lightLinearFalloff = 0.09f;
		float     lightQuadraticFalloff = 0.032f;
		int mode = 0;
		
		// These are our application / scene level uniforms that don't necessarily update
		// every frame
		shader->SetUniform("u_LightPos", lightPos);
		shader->SetUniform("u_LightCol", lightCol);
		shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		shader->SetUniform("u_AmbientCol", ambientCol);
		shader->SetUniform("u_AmbientStrength", ambientPow);
		shader->SetUniform("u_LightAttenuationConstant", 1.0f);
		shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);

		PostEffect* basicEffect;

		int activeEffect = 0;
		std::vector<PostEffect*> effects;

		GreyscaleEffect* greyscaleEffect;

		BloomEffect* bloomEffect;

		bool texOn = true;
		int rim = 0;

		std::vector<ShaderMaterial::sptr> mats;
#pragma region TEXTURE LOADING

		// Load some textures from files
		Texture2D::sptr stone = Texture2D::LoadFromFile("images/Stone_001_Diffuse.png");
		Texture2D::sptr stoneSpec = Texture2D::LoadFromFile("images/Stone_001_Specular.png");
		Texture2D::sptr grass = Texture2D::LoadFromFile("images/grass.jpg");
		Texture2D::sptr noSpec = Texture2D::LoadFromFile("images/grassSpec.png");
		Texture2D::sptr box = Texture2D::LoadFromFile("images/box.bmp");
		Texture2D::sptr boxSpec = Texture2D::LoadFromFile("images/box-reflections.bmp");
		Texture2D::sptr simpleFlora = Texture2D::LoadFromFile("images/SimpleFlora.png");

		Texture2D::sptr shrineCol = Texture2D::LoadFromFile("images/reyebl.png");
		Texture2D::sptr crystalNor = Texture2D::LoadFromFile("images/Crystal_Normal.png");
		Texture2D::sptr crystalDif = Texture2D::LoadFromFile("images/Crystal_Albedo.png");
		Texture2D::sptr crystalGlow = Texture2D::LoadFromFile("images/Crystal_Emission.png");
		//LUT3D testCube("cubes/BrightenedCorrection.cube");



		// Creating an empty texture
		Texture2DDescription desc = Texture2DDescription();
		desc.Width = 1;
		desc.Height = 1;
		desc.Format = InternalFormat::RGB8;
		Texture2D::sptr texture2 = Texture2D::Create(desc);
		// Clear it with a white colour
		texture2->Clear();


		// Load the cube map
		//TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/sample.jpg");
		TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/ToonSky.jpg");
#pragma endregion

		int width, height;
		glfwGetWindowSize(BackendHandler::window, &width, &height);

		// We'll add some ImGui controls to control our shader
		BackendHandler::imGuiCallbacks.push_back([&]() {
			if (ImGui::Button("No Lighting")) {
				mode = 1;
				shader->SetUniform("u_Mode", mode);
				activeEffect = 0;
			}
			if (ImGui::Button("Ambient Only")) {
				mode = 2;
				shader->SetUniform("u_Mode", mode);
				activeEffect = 0;
			}
			if (ImGui::Button("Specular Only")) {
				mode = 3;
				shader->SetUniform("u_Mode", mode);
				activeEffect = 0;
			}//
			if (ImGui::Button("Ambient + Specular")) {
				mode = 0;
				shader->SetUniform("u_Mode", mode);
				activeEffect = 0;
			}
			if (ImGui::Button("Ambient + Specular + Bloom")) {
				mode = 7;
				shader->SetUniform("u_Mode", mode);
				activeEffect = 1;
			}
			if (ImGui::CollapsingHeader("Effect controls")) {
				BloomEffect* temp = (BloomEffect*)effects[activeEffect];
				float threshold = temp->GetThreshold();
				int pass = temp->GetPasses();
								
				if (ImGui::SliderFloat("Brightness Threshold", &threshold, 0.0f, 1.0f))
				{//
					temp->SetThreshold(threshold);
				}
				
				if (ImGui::SliderInt("Blur", &pass, 0, 10))
				{
					temp->SetPasses(pass);
				}
			}
		
			if (ImGui::Button("Toggle Texture")) {
				if (texOn)
				{
					texOn = false;
					for (int i = 0; i < mats.size(); i++) {
						mats[i]->Set("s_Diffuse", texture2);
					}
					mats[5]->Set("s_Diffuse2", texture2);
				}
				else {
					texOn = true;
					mats[0]->Set("s_Diffuse", stone);
					mats[1]->Set("s_Diffuse", grass);
					mats[2]->Set("s_Diffuse", box);
					mats[3]->Set("s_Diffuse", simpleFlora);
					mats[4]->Set("s_Diffuse", shrineCol);
					mats[5]->Set("s_Diffuse", crystalNor);
					mats[6]->Set("s_Diffuse", crystalNor);

					mats[5]->Set("s_Diffuse2", crystalGlow);

				}
				
			}
			if (ImGui::Button("Toggle Rim Lighting")) {
				if (rim)
				{
					rim = 0;
				}
				else {
					rim = 1;
				}
				shader->SetUniform("u_rim", rim);

			}
			/*if (ImGui::CollapsingHeader("Environment generation"))
			{
				if (ImGui::Button("Regenerate Environment", ImVec2(200.0f, 40.0f)))
				{
					EnvironmentGenerator::RegenerateEnvironment();
				}
			}*/
			if (ImGui::CollapsingHeader("Scene Level Lighting Settings"))
			{
				if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
					shader->SetUniform("u_AmbientCol", ambientCol);
				}
				if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
					shader->SetUniform("u_AmbientStrength", ambientPow);
				}
			}
			if (ImGui::CollapsingHeader("Light Level Lighting Settings"))
			{
				if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
					shader->SetUniform("u_LightPos", lightPos);
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
					shader->SetUniform("u_LightCol", lightCol);
				}
				if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				}
				if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
				}
				if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
				}
			}

			ImGui::Text("Q/E -> Yaw\nLeft/Right -> Roll\nUp/Down -> Pitch\nY -> Toggle Mode");
		
			minFps = FLT_MAX;
			maxFps = 0;
			avgFps = 0;
			for (int ix = 0; ix < 128; ix++) {
				if (fpsBuffer[ix] < minFps) { minFps = fpsBuffer[ix]; }
				if (fpsBuffer[ix] > maxFps) { maxFps = fpsBuffer[ix]; }
				avgFps += fpsBuffer[ix];
			}
			ImGui::PlotLines("FPS", fpsBuffer, 128);
			ImGui::Text("MIN: %f MAX: %f AVG: %f", minFps, maxFps, avgFps / 128.0f);
			});

		#pragma endregion 

		// GL states
		glEnable(GL_DEPTH_TEST);
		//glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL); // New 

		

		///////////////////////////////////// Scene Generation //////////////////////////////////////////////////
		#pragma region Scene Generation
		
		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		// Create a scene, and set it to be the active scene in the application
		GameScene::sptr scene = GameScene::Create("test");
		Application::Instance().ActiveScene = scene;

		// We can create a group ahead of time to make iterating on the group faster
		entt::basic_group<entt::entity, entt::exclude_t<>, entt::get_t<Transform>, RendererComponent> renderGroup =
			scene->Registry().group<RendererComponent>(entt::get_t<Transform>());
		

		// Create a material and set some properties for it
		ShaderMaterial::sptr stoneMat = ShaderMaterial::Create();  
		stoneMat->Shader = shader;
		stoneMat->Set("s_Diffuse", stone);
		stoneMat->Set("s_Specular", stoneSpec);
		stoneMat->Set("u_Shininess", 2.0f);
		stoneMat->Set("u_TextureMix", 0.0f); 
		
		mats.push_back(stoneMat);

		ShaderMaterial::sptr grassMat = ShaderMaterial::Create();
		grassMat->Shader = shader;
		grassMat->Set("s_Diffuse", grass);
		grassMat->Set("s_Specular", noSpec);
		grassMat->Set("u_Shininess", 2.0f);
		grassMat->Set("u_TextureMix", 0.0f);
		mats.push_back(grassMat);


		ShaderMaterial::sptr boxMat = ShaderMaterial::Create();
		boxMat->Shader = shader;
		boxMat->Set("s_Diffuse", box);
		boxMat->Set("s_Specular", boxSpec);
		boxMat->Set("u_Shininess", 8.0f);
		boxMat->Set("u_TextureMix", 0.0f);
		mats.push_back(boxMat);

		ShaderMaterial::sptr simpleFloraMat = ShaderMaterial::Create();
		simpleFloraMat->Shader = shader;
		simpleFloraMat->Set("s_Diffuse", simpleFlora);
		simpleFloraMat->Set("s_Specular", noSpec);
		simpleFloraMat->Set("u_Shininess", 8.0f);
		simpleFloraMat->Set("u_TextureMix", 0.0f);
		mats.push_back(simpleFloraMat);

		ShaderMaterial::sptr shrineMat = ShaderMaterial::Create();
		shrineMat->Shader = shader;
		shrineMat->Set("s_Diffuse", shrineCol);
		shrineMat->Set("s_Specular", noSpec);
		shrineMat->Set("u_Shininess", 8.0f);
		shrineMat->Set("u_TextureMix", 0.0f);
		mats.push_back(shrineMat);

		ShaderMaterial::sptr dcrystalMat = ShaderMaterial::Create();
		dcrystalMat->Shader = shader;
		dcrystalMat->Set("s_Diffuse", crystalNor);
		dcrystalMat->Set("s_Diffuse2", crystalGlow);
		dcrystalMat->Set("s_Specular", crystalDif);
		dcrystalMat->Set("u_Shininess", 8.0f);
		dcrystalMat->Set("u_TextureMix", 0.6f);
		mats.push_back(dcrystalMat);

		ShaderMaterial::sptr crystalMat = ShaderMaterial::Create();
		crystalMat->Shader = shader;
		crystalMat->Set("s_Diffuse", crystalNor);
		crystalMat->Set("s_Diffuse2", crystalDif);
		crystalMat->Set("s_Specular", crystalGlow);
		crystalMat->Set("u_Shininess", 8.0f);
		crystalMat->Set("u_TextureMix", 0.7f);
		mats.push_back(crystalMat);

		int spin = 0;

		GameObject obj1 = scene->CreateEntity("Ground"); 
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/plane.obj");
			obj1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(stoneMat);
			obj1.get<Transform>().SetLocalScale(0.35f, 0.35f, 1.0f);

		}

		GameObject obj2 = scene->CreateEntity("shrine");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/shrine.obj");
			obj2.emplace<RendererComponent>().SetMesh(vao).SetMaterial(shrineMat);
			obj2.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			obj2.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);

		}
		GameObject obj3 = scene->CreateEntity("crystal_mid");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/crystal.obj");
			obj3.emplace<RendererComponent>().SetMesh(vao).SetMaterial(crystalMat);
			obj3.get<Transform>().SetLocalPosition(0.0f, 0.0f, 5.0f);
			obj3.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);

			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(obj3);
			pathing->Points.push_back({ 0.0f, 0.0f, 5.5f });
			pathing->Points.push_back({ 0.0f, 0.0f, 5.0f });


			pathing->Speed = 0.25f;
		}
		
		GameObject obj5 = scene->CreateEntity("crystal_left");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/crystal.obj");
			obj5.emplace<RendererComponent>().SetMesh(vao).SetMaterial(dcrystalMat);
			obj5.get<Transform>().SetLocalPosition(4.5f, -4.0f, 1.0f);
			obj5.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			
			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(obj5);
			pathing->Points.push_back({ 4.5f, 4.0f, 1.0f });
			pathing->Points.push_back({ 4.5f, -4.0f, 1.0f });

			pathing->Speed = 2.0f;
		}
		GameObject obj6 = scene->CreateEntity("crystal_up");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/crystal.obj");
			obj6.emplace<RendererComponent>().SetMesh(vao).SetMaterial(dcrystalMat);
			obj6.get<Transform>().SetLocalPosition(4.5f, 4.0f, 1.0f);
			obj6.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);

			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(obj6);
			pathing->Points.push_back({ -4.25f, 4.0f, 1.0f });
			pathing->Points.push_back({ 4.5f, 4.0f, 1.0f });

			pathing->Speed = 2.0f;
		}
		GameObject obj7 = scene->CreateEntity("crystal_right");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/crystal.obj");
			obj7.emplace<RendererComponent>().SetMesh(vao).SetMaterial(dcrystalMat);
			obj7.get<Transform>().SetLocalPosition(-4.25f, -4.25f, 1.0f);
			obj7.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);

			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(obj7);
			pathing->Points.push_back({ 4.5f, -4.0f, 1.0f });
			pathing->Points.push_back({ -4.25f, -4.25f, 1.0f });

			pathing->Speed = 2.0f;
		}

		GameObject obj8 = scene->CreateEntity("crystal_down");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/crystal.obj");
			obj8.emplace<RendererComponent>().SetMesh(vao).SetMaterial(dcrystalMat);
			obj8.get<Transform>().SetLocalPosition(-4.25f, 4.0f, 1.0f);
			obj8.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);

			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(obj8);
			pathing->Points.push_back({ -4.25f, -4.25f, 1.0f });
			pathing->Points.push_back({ -4.25f, 4.0f, 1.0f });

			pathing->Speed = 2.0f;
		}
	
		// Create an object to be our camera
		GameObject cameraObject = scene->CreateEntity("Camera");
		{
			cameraObject.get<Transform>().SetLocalPosition(0, 7, 7).LookAt(glm::vec3(0, 0, 0));

			// We'll make our camera a component of the camera object
			Camera& camera = cameraObject.emplace<Camera>();// Camera::Create();
			camera.SetPosition(glm::vec3(0, 3, 3));
			camera.SetUp(glm::vec3(0, 0, 1));
			camera.LookAt(glm::vec3(0));
			camera.SetFovDegrees(90.0f); // Set an initial FOV
			camera.SetOrthoHeight(3.0f);
			BehaviourBinding::Bind<CameraControlBehaviour>(cameraObject);
		}



		GameObject framebufferObject = scene->CreateEntity("Basic Effect");
		{
			basicEffect = &framebufferObject.emplace<PostEffect>();
			basicEffect->Init(width, height);
		}
		GameObject greyscaleEffectObject = scene->CreateEntity("greyscale Effect");
		{
			greyscaleEffect = &greyscaleEffectObject.emplace<GreyscaleEffect>();
			greyscaleEffect->Init(width, height);
		}
		effects.push_back(greyscaleEffect);

		GameObject bloomEffectObject = scene->CreateEntity("Bloom Effect");
		{
			bloomEffect = &bloomEffectObject.emplace<BloomEffect>();
			bloomEffect->Init(width, height);
		}
		effects.push_back(bloomEffect);
		

		#pragma endregion 
		///////////////////////////////////////////////////////////////////////////////////////////

		/////////////////////////////////// SKYBOX ///////////////////////////////////////////////
		{
			// Load our shaders
			Shader::sptr skybox = std::make_shared<Shader>();
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.vert.glsl", GL_VERTEX_SHADER);
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.frag.glsl", GL_FRAGMENT_SHADER);
			skybox->Link();

			ShaderMaterial::sptr skyboxMat = ShaderMaterial::Create();
			skyboxMat->Shader = skybox;  
			skyboxMat->Set("s_Environment", environmentMap);
			skyboxMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));
			skyboxMat->RenderLayer = 100;

			MeshBuilder<VertexPosNormTexCol> mesh;
			MeshFactory::AddIcoSphere(mesh, glm::vec3(0.0f), 1.0f);
			MeshFactory::InvertFaces(mesh);
			VertexArrayObject::sptr meshVao = mesh.Bake();
			
			GameObject skyboxObj = scene->CreateEntity("skybox");  
			skyboxObj.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat);
		}
		////////////////////////////////////////////////////////////////////////////////////////


		// We'll use a vector to store all our key press events for now (this should probably be a behaviour eventually)
		std::vector<KeyPressWatcher> keyToggles;
		{
			// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
			// how this is implemented. Note that the ampersand here is capturing the variables within
			// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
			// use std::bind
			keyToggles.emplace_back(GLFW_KEY_T, [&]() { cameraObject.get<Camera>().ToggleOrtho(); });

			controllables.push_back(obj2);

			keyToggles.emplace_back(GLFW_KEY_KP_ADD, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao++;
				if (selectedVao >= controllables.size())
					selectedVao = 0;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});
			keyToggles.emplace_back(GLFW_KEY_KP_SUBTRACT, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao--;
				if (selectedVao < 0)
					selectedVao = controllables.size() - 1;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});

			keyToggles.emplace_back(GLFW_KEY_Y, [&]() {
				auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
				behaviour->Relative = !behaviour->Relative;
				});
		}

		// Initialize our timing instance and grab a reference for our use
		Timing& time = Timing::Instance();
		time.LastFrame = glfwGetTime();

		///// Game loop /////
		while (!glfwWindowShouldClose(BackendHandler::window)) {
			glfwPollEvents();

			// Update the timing
			time.CurrentFrame = glfwGetTime();
			time.DeltaTime = static_cast<float>(time.CurrentFrame - time.LastFrame);

			time.DeltaTime = time.DeltaTime > 1.0f ? 1.0f : time.DeltaTime;

			obj3.get<Transform>().SetLocalRotation(90.0f, 0.0f, spin % 360);
			obj5.get<Transform>().SetLocalRotation(90.0f, 0.0f, -(2*spin % 360));
			obj6.get<Transform>().SetLocalRotation(90.0f, 0.0f, -(2*spin % 360));
			obj7.get<Transform>().SetLocalRotation(90.0f, 0.0f, -(2*spin % 360));
			obj8.get<Transform>().SetLocalRotation(90.0f, 0.0f, -(2*spin % 360));
			spin++;

			// Update our FPS tracker data
			fpsBuffer[frameIx] = 1.0f / time.DeltaTime;
			frameIx++;
			if (frameIx >= 128)
				frameIx = 0;

			// We'll make sure our UI isn't focused before we start handling input for our game
			if (!ImGui::IsAnyWindowFocused()) {
				// We need to poll our key watchers so they can do their logic with the GLFW state
				// Note that since we want to make sure we don't copy our key handlers, we need a const
				// reference!
				for (const KeyPressWatcher& watcher : keyToggles) {
					watcher.Poll(BackendHandler::window);
				}
			}

			// Iterate over all the behaviour binding components
			scene->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
				// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
				for (const auto& behaviour : binding.Behaviours) {
					if (behaviour->Enabled) {
						behaviour->Update(entt::handle(scene->Registry(), entity));
					}
				}
			});

			// Clear the screen
			basicEffect->Clear();
			/*greyscaleEffect->Clear();
			sepiaEffect->Clear();*/
			for (int i = 0; i < effects.size(); i++)
			{
				effects[i]->Clear();
			}


			glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
			glEnable(GL_DEPTH_TEST);
			glClearDepth(1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Update all world matrices for this frame
			scene->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
				t.UpdateWorldMatrix();
			});
			
			// Grab out camera info from the camera object
			Transform& camTransform = cameraObject.get<Transform>();
			glm::mat4 view = glm::inverse(camTransform.LocalTransform());
			glm::mat4 projection = cameraObject.get<Camera>().GetProjection();
			glm::mat4 viewProjection = projection * view;
						
			// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
			renderGroup.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
				// Sort by render layer first, higher numbers get drawn last
				if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
				if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

				// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
				if (l.Material->Shader < r.Material->Shader) return true;
				if (l.Material->Shader > r.Material->Shader) return false;

				// Sort by material pointer last (so we can minimize switching between materials)
				if (l.Material < r.Material) return true;
				if (l.Material > r.Material) return false;
				
				return false;
			});

			// Start by assuming no shader or material is applied
			Shader::sptr current = nullptr;
			ShaderMaterial::sptr currentMat = nullptr;

			basicEffect->BindBuffer(0);

			// Iterate over the render group components and draw them
			renderGroup.each( [&](entt::entity e, RendererComponent& renderer, Transform& transform) {
				// If the shader has changed, set up it's uniforms
				if (current != renderer.Material->Shader) {
					current = renderer.Material->Shader;
					current->Bind();
					BackendHandler::SetupShaderForFrame(current, view, projection);
				}
				// If the material has changed, apply it
				if (currentMat != renderer.Material) {
					currentMat = renderer.Material;
					currentMat->Apply();
				}
				// Render the mesh
				BackendHandler::RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
			});

			basicEffect->UnbindBuffer();

			effects[activeEffect]->ApplyEffect(basicEffect);
			
			effects[activeEffect]->DrawToScreen();
			
			// Draw our ImGui content
			BackendHandler::RenderImGui();

			scene->Poll();
			glfwSwapBuffers(BackendHandler::window);
			time.LastFrame = time.CurrentFrame;
		}

		// Nullify scene so that we can release references
		Application::Instance().ActiveScene = nullptr;
		//Clean up the environment generator so we can release references
		EnvironmentGenerator::CleanUpPointers();
		BackendHandler::ShutdownImGui();
	}	

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}