#include "MiniWorld.hpp"

#include <AssetLibrary/Model.hpp>
#include <AssetLibrary/Image.hpp>
#include <InlineMath.hpp>

#include <array>

using namespace inl;

inline float rand2() {
	return (rand() / float(RAND_MAX)) * 2 - 1;
}

MiniWorld::MiniWorld(inl::gxeng::GraphicsEngine * graphicsEngine) {
	using namespace inl::gxeng;

	m_graphicsEngine = graphicsEngine;

	m_worldScene.reset(m_graphicsEngine->CreateScene("World"));
	m_sun.SetColor(Vec3(1, 1, 0.75));
	m_sun.SetDirection(Vec3(1, 1, 1));
	m_worldScene->GetDirectionalLights().Add(&m_sun);

	m_camera.reset(m_graphicsEngine->CreatePerspectiveCamera("WorldCam"));
	m_camera->SetTargeted(true);
	m_camera->SetPosition({0, 0, 8});
	m_camera->SetTarget({ 0, 0, 0 });
	m_camera->SetUpVector({ 0, 1, 0 });
	m_camera->SetNearPlane(0.5);
	m_camera->SetFarPlane(20.0);

	{
		inl::asset::Model model("monkey.dae");

		auto modelVertices = model.GetVertices<Position<0>, Normal<0>, TexCoord<0>>(0);
		std::vector<unsigned> modelIndices = model.GetIndices(0);

		m_cubeMesh.reset(m_graphicsEngine->CreateMesh());
		m_cubeMesh->Set(modelVertices.data(), modelVertices.size(), modelIndices.data(), modelIndices.size());
	}

	{
		using PixelT = Pixel<ePixelChannelType::INT8_NORM, 3, ePixelClass::LINEAR>;
		inl::asset::Image img("monkey.png");
		assert(img.GetChannelCount() == 3 && img.GetType() == inl::asset::eChannelType::INT8);

		m_texture.reset(m_graphicsEngine->CreateImage());
		m_texture->SetLayout(img.GetWidth(), img.GetHeight(), ePixelChannelType::INT8_NORM, 3, ePixelClass::LINEAR);
		m_texture->Update(0, 0, img.GetWidth(), img.GetHeight(), img.GetData(), PixelT::Reader());
	}

	srand(time(nullptr));

	const float extent = 5;
	const int count = 6;
	for (int i = 0; i < count; i++) {
		std::unique_ptr<inl::gxeng::MeshEntity> entity(new inl::gxeng::MeshEntity());
		entity->SetMesh(m_cubeMesh.get());
		// texture won't cut it anymore
		// one must use materials
		// null materials won't cut it now, and will never do so
		// i was lazy to create a material for the entities, so this code should never run, it will die anyway
		// use QC simulator for testing
		// this project is kinda obsolete anyway
		assert(false);
		entity->SetMaterial(nullptr);
		Vec3 pos;
		pos.x = float((i + 0.5f)*extent) / count - extent*0.5f;
		pos.y = 0;
		pos.z = 0;
		entity->SetPosition(pos);

		m_worldScene->GetMeshEntities().Add(entity.get());
		m_staticEntities.push_back(std::move(entity));
		m_velocities.push_back(Vec3(rand2(), rand2(), 0));
	}
}

void MiniWorld::UpdateWorld(float elapsed) {
	const float boundary = 3;
	assert(m_staticEntities.size() == m_velocities.size());
	for (int i = 0; i < m_staticEntities.size(); i++) {
		auto& currEntity = m_staticEntities[i];
		auto& currVel = m_velocities[i];

		currVel += Vec3(rand2(), rand2(), 0)*5.f*elapsed;

		const float maxSpeed = 2.5f;
		if (currVel.Length() > maxSpeed) {
			currVel = currVel.Normalized() * maxSpeed;
		}

		auto newPos = currEntity->GetPosition() + currVel*elapsed;

		if (newPos.x > boundary || newPos.x < -boundary) {
			currVel.x *= -1;
			newPos = currEntity->GetPosition();
		}
		if (newPos.y > boundary || newPos.y < -boundary) {
			currVel.y *= -1;
			newPos = currEntity->GetPosition();
		}

		currEntity->SetPosition(newPos);
		auto tmp = currVel.Normalized();
		currEntity->SetRotation(currEntity->GetRotation() * Quat::AxisAngle(tmp, 1.5f*elapsed));
	}
}

void MiniWorld::SetAspectRatio(float ar) {
	m_camera->SetFOVAspect(60.f / 180.f * 3.1419f, ar);
}

void MiniWorld::RenderWorld(float elapsed) {
	m_graphicsEngine->Update(elapsed);
}
