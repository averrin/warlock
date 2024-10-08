#include <vector>
#include <cmath>
#include <algorithm>

class Material {
public:
    double thermalConductivity;
    double specificHeatCapacity;
    double density;

    Material(double tc, double shc, double d) 
        : thermalConductivity(tc), specificHeatCapacity(shc), density(d) {}
};

class Environment {
public:
    double temperature;
    double airFlow;

    Environment(double temp, double flow) : temperature(temp), airFlow(flow) {}
};

class Component {
public:
    double temperature;
    double volume;
    Material* material;

    Component(double temp, double vol, Material* mat) 
        : temperature(temp), volume(vol), material(mat) {}

    double getThermalMass() const {
        return volume * material->density * material->specificHeatCapacity;
    }
};

class Frame {
public:
    std::vector<Component*> components;
    Material* material;
    double surfaceArea;

    Frame(Material* mat, double area) : material(mat), surfaceArea(area) {}
};

class ThermalSystem {
private:
    Environment* environment;
    std::vector<Frame*> frames;
    const double stefanBoltzmannConstant = 5.67e-8;
    const double timeStep = 0.1; // seconds

    double calculateConduction(const Component* c1, const Component* c2, double contactArea) {
        double thermalConductance = (c1->material->thermalConductivity + c2->material->thermalConductivity) / 2.0;
        return thermalConductance * contactArea * (c2->temperature - c1->temperature) / 
               std::max(0.01, std::sqrt(c1->volume) + std::sqrt(c2->volume));
    }

    double calculateConvection(const Component* c, double surfaceArea) {
        double heatTransferCoeff = 10.0 + 2.0 * environment->airFlow;  // Simplified coefficient
        return heatTransferCoeff * surfaceArea * (environment->temperature - c->temperature);
    }

    double calculateRadiation(const Component* c, double surfaceArea) {
        double emissivity = 0.95;  // Assume high emissivity for most materials
        return emissivity * stefanBoltzmannConstant * surfaceArea * 
               (std::pow(environment->temperature, 4) - std::pow(c->temperature, 4));
    }

public:
    ThermalSystem(Environment* env) : environment(env) {}

    void addFrame(Frame* frame) {
        frames.push_back(frame);
    }

    void update() {
        for (auto frame : frames) {
            for (auto component : frame->components) {
                double totalHeatTransfer = 0.0;

                // Conduction with other components in the frame
                for (auto other : frame->components) {
                    if (other != component) {
                        double contactArea = std::min(component->volume, other->volume) / 
                                             std::max(component->volume, other->volume) * frame->surfaceArea;
                        totalHeatTransfer += calculateConduction(component, other, contactArea);
                    }
                }

                // Convection and radiation with environment
                double componentSurfaceArea = std::pow(component->volume, 2.0/3.0);
                totalHeatTransfer += calculateConvection(component, componentSurfaceArea);
                totalHeatTransfer += calculateRadiation(component, componentSurfaceArea);

                // Update component temperature
                double temperatureChange = totalHeatTransfer / component->getThermalMass() * timeStep;
                component->temperature += temperatureChange;
            }
        }
    }
};

// Example usage
int ex_main() {
    Material* aluminum = new Material(205.0, 897.0, 2700.0);
    Material* steel = new Material(50.2, 490.0, 7850.0);

    Environment* env = new Environment(20.0, 2.0);  // 20°C, light breeze

    Component* c1 = new Component(30.0, 0.001, aluminum);  // Small, warm aluminum component
    Component* c2 = new Component(15.0, 0.005, steel);     // Larger, cool steel component

    Frame* frame = new Frame(aluminum, 0.1);  // Aluminum frame with 0.1 m² surface area
    frame->components.push_back(c1);
    frame->components.push_back(c2);

    ThermalSystem system(env);
    system.addFrame(frame);

    // Simulation loop
    for (int i = 0; i < 1000; ++i) {
        system.update();
        printf("Step %d: Component 1 temp = %.2f°C, Component 2 temp = %.2f°C\n", 
               i, c1->temperature, c2->temperature);
        // Here you would update your game state, render graphics, etc.
    }

    // Clean up
    delete aluminum;
    delete steel;
    delete env;
    delete c1;
    delete c2;
    delete frame;

    return 0;
}
