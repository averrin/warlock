#pragma once
#include <game/components/frame.hpp>
#include <game/system.hpp>
#include <vector>
#include <deque>
#include <string>
#include <map>
#include <utils/entt.hpp>
#include <algorithm>

class Material {
public:
    float thermalConductivity;
    float specificHeatCapacity;
    float density;

    Material(float tc, float shc, float d) 
        : thermalConductivity(tc), specificHeatCapacity(shc), density(d) {}
};

class ThermalSystem : public System {
  Environment* environment;
public:
  void fixedUpdate() override;
  ThermalSystem() : System(50) {
    Material* aluminum = new Material(205.0, 897.0, 2700.0);
    Material* steel = new Material(50.2, 490.0, 7850.0);
    Material* copper = new Material(401.0f, 385.0f, 8960.0f);
    Material* titanium = new Material(21.9f, 523.0f, 4500.0f);
    Material* plastic = new Material(0.19f, 1500.0f, 1400.0f);  // PVC
    Material* glass = new Material(1.05f, 840.0f, 2500.0f);

    materials[ComponentMaterial::ALUMINIUM] = aluminum;
    materials[ComponentMaterial::STEEL] = steel;
    materials[ComponentMaterial::COPPER] = copper;
    materials[ComponentMaterial::TITANIUM] = titanium;
    materials[ComponentMaterial::PLASTIC] = plastic;
    materials[ComponentMaterial::GLASS] = glass;
  }

  std::map<ComponentMaterial, Material*> materials;
  std::map<ComponentSize, float> volumes = {
    {ComponentSize::S, 0.001},
    {ComponentSize::M, 0.01},
    {ComponentSize::L, 0.1}
  };

  std::map<FrameSize, float> surfaceAreas = {
    {FrameSize::S, 10},
    {FrameSize::M, 40},
    {FrameSize::L, 150},
    {FrameSize::G, 600}
  };

  const float stefanBoltzmannConstant = 5.67e-8;
  const float timeStep = 0.05; // seconds

    float calculateConduction(Component* c1, Component* c2, float contactArea) {
      auto m1 = materials[c1->material];
      auto m2 = materials[c2->material];
      auto t1 = c1->data.get<float>("temp");
      auto t2 = c2->data.get<float>("temp");
      auto v1 = volumes[c1->size];
      auto v2 = volumes[c2->size];
      float thermalConductance = (m1->thermalConductivity + m2->thermalConductivity) / 2.0;
      return thermalConductance * contactArea * (t2 - t1) / 
             std::max(0.01f, std::sqrt(v1) + std::sqrt(v2));
    }

    float getThermalMass(Component* c) {
        auto m = materials[c->material];
        auto volume = volumes[c->size];
        return volume * m->density * m->specificHeatCapacity;
    }

    float calculateConvection(Component* c, float surfaceArea) {
    }

    float calculateRadiation(Component* c, float surfaceArea) {
      auto temp = c->data.get<float>("temp");
      float emissivity = 0.95;  // Assume high emissivity for most materials
      return emissivity * stefanBoltzmannConstant * surfaceArea * 
             (std::pow(environment->temperature, 4) - std::pow(temp, 4));
    }
};

