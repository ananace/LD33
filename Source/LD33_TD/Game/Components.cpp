#include "Components.hpp"
#include <Base/VectorMath.hpp>

Game::Physical::Physical() : Kunlaboro::Component("Game.Physical"),
	Radius(0), Blocking(false)
{

}

void Game::Physical::addedToEntity()
{
	requestMessage("GetPosition", [this](Kunlaboro::Message& msg) { msg.handle(Position); }, true);
	requestMessage("GetVelocity", [this](Kunlaboro::Message& msg) { msg.handle(Velocity); }, true);
	requestMessage("GetRadius", [this](Kunlaboro::Message& msg) { msg.handle(Radius); }, true);
	requestMessage("SetPosition", [this](const Kunlaboro::Message& msg) { Position = msg.payload.get<sf::Vector2f>(); }, true);
	requestMessage("SetVelocity", [this](const Kunlaboro::Message& msg) { Velocity = msg.payload.get<sf::Vector2f>(); }, true);
	requestMessage("SetRadius", [this](const Kunlaboro::Message& msg) { Radius = msg.payload.get<float>(); }, true);

	requestMessage("Game.Physical.Find", [this](Kunlaboro::Message& msg) {
		auto question = msg.payload.get<std::tuple<sf::Vector2f, float>>();

		if (!Blocking && VMath::DistanceSqrt(Position, std::get<0>(question)) < std::get<1>(question))
			msg.handle(Position);
	});
	requestMessage("Game.Physical.Blocking", [this](const Kunlaboro::Message& msg){
		if (!Blocking)
			return;

		auto vec = msg.payload.get<std::vector<Physical*>*>();
		vec->push_back(this);
	});

	/// \FIXME Move to a PhysicalManager, with a single Tick function.
	requestMessage("LD33.Tick", [this](const Kunlaboro::Message& msg) {
		float dt = msg.payload.get<float>();
		Position += Velocity * dt;
	});
}
