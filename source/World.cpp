#include "World.hpp"

#include <climits>
#include <cmath>
#include <vector>
#include <cstdio>

#include <SDL2/SDL_timer.h>


#include <engine/renderer/GWENRenderer.hpp>

#include <assets/map/MapLoader.hpp>
#include <assets/map/MapListLoader.hpp>

#include <engine/renderer/Renderer.hpp>
#include <engine/env/Environment.hpp>
#include <engine/Camera.hpp>

#include <engine/component/Health.hpp>
#include <engine/component/Transform.hpp>
#include <engine/component/Trigger.hpp>
#include <engine/component/SoundSource.hpp>
#include <engine/component/LightSource.hpp>
#include <engine/component/Player.hpp>

#include <engine/core/math/Math.hpp>
#include <engine/core/math/Vector2f.hpp>
#include <engine/core/math/Vector3f.hpp>
#include <engine/core/event/Dispatcher.hpp>
#include <engine/core/state/GameState.hpp>

#include "Input.hpp"
#include "Portal.hpp"
#include "Window.hpp"
#include "WorldHelper.hpp"

namespace glPortal {

float World::gravity = GRAVITY;

World::World() :
  scene(nullptr),
  gameTime(0),
  lastUpdateTime(0),
  renderer(nullptr),
  config(Environment::getConfig()) {
  stateFunctionStack.push(&GameState::handleRunning);
  stateFunctionStack.push(&GameState::handleSplash);
}

World::~World() = default;

void World::create() {
  mapList = MapListLoader::getMapList();
  lastUpdateTime = SDL_GetTicks();

  renderer = new Renderer();

  std::random_device rd;
  generator = std::mt19937(rd());

  std::string mapPath = config.mapPath;
  if (mapPath.length() > 0) {
    loadSceneFromPath(mapPath);
    return;
  }

  try {
    std::string map = config.map;
    loadScene(map);
    System::Log(Info) << "Custom map loaded";
  } catch (const std::out_of_range& e) {
    System::Log(Info) << "No custom map found loading default map.";
    loadScene(mapList[currentLevel]);
  }

}


void World::setRendererWindow(Window *win) {
  renderer->setViewport(win);
  window = win;
}

void World::destroy() {
  delete renderer;
  delete scene;
}

void World::loadScene(const std::string &path) {
  
  delete scene;
  currentScenePath = path;
  scene = MapLoader::getScene(path);

  if (justStarted) {
    Entity &player = *scene->player;
    Player &plr = player.getComponent<Player>();
    plr.frozen = true;
    scene->screen->enabled = true;
    justStarted = false;
  }

  scene->world = this;
  plrSys.setScene(scene);
  phys.setScene(scene);
  scene->physics.world->setDebugDrawer(&pdd);
  scene->physics.setGravity(0, -9.8, 0);
  prtl.setScene(scene);
  renderer->setScene(scene);

  Environment::dispatcher.dispatch(Event::loadScene);
}


void World::loadSceneFromPath(const std::string &path) {
  
  delete scene;
  currentScenePath = path;
  scene = MapLoader::getSceneFromPath(path);

  if (justStarted) {
    Entity &player = *scene->player;
    Player &plr = player.getComponent<Player>();
    plr.frozen = true;
    scene->screen->enabled = true;
    justStarted = false;
  }

  scene->world = this;
  plrSys.setScene(scene);
  phys.setScene(scene);
  scene->physics.world->setDebugDrawer(&pdd);
  scene->physics.setGravity(0, -9.8, 0);
  prtl.setScene(scene);
  renderer->setScene(scene);

  Environment::dispatcher.dispatch(Event::loadScene);
}

void World::update(double dtime) {
  gameTime += dtime;
  
  Entity &player = *scene->player;
  Health &plrHealth = player.getComponent<Health>();
  Transform &plrTform = player.getComponent<Transform>();
  Player &plrComp = player.getComponent<Player>();

  if (not plrHealth.isAlive()) {
    plrTform.setPosition(scene->start->getComponent<Transform>().getPosition());
    plrHealth.revive();
    hidePortals();
  }

  plrSys.update(dtime);
  phys.update(dtime);
  prtl.update(dtime);



 
  scene->camera.setPerspective();
  int vpWidth, vpHeight;
  renderer->getViewport()->getSize(&vpWidth, &vpHeight);
  scene->camera.setAspect((float)vpWidth / vpHeight);
  scene->camera.setPosition(plrTform.getPosition() + Vector3f(0, plrTform.getScale().y/2, 0));
  scene->camera.setOrientation(plrComp.getHeadOrientation());

  
  float distToEnd = (scene->end->getComponent<Transform>().getPosition() -
                     plrTform.getPosition()).length();
  if (distToEnd < 1) {
    if (currentLevel + 1 < mapList.size()) {
      currentLevel++;
      loadScene(mapList[currentLevel]);
    } else {
      scene->screen->enabled = true;
      scene->screen->title   = "Game Over";
      scene->screen->text    = "Hit q to quit the game.";
    }
  }
}

void World::shootPortal(int button) {
  WorldHelper::shootPortal(button, scene);
}

Renderer* World::getRenderer() const {
  return renderer;
}


void World::render(double dtime) {
  renderer->render(dtime, scene->camera);
  if (isPhysicsDebugEnabled) {
    scene->physics.world->debugDrawWorld();
    pdd.render(scene->camera);
  }
  window->gwenCanvas->RenderCanvas();
  window->gwenRenderer->End();
}

Entity& World::getPlayer() {
  return *scene->player;
}

void World::hidePortals() {
  WorldHelper::closePortals(scene);
}

} /* namespace glPortal */
