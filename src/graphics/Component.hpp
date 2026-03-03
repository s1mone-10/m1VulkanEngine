/* Copyright (c) 2025 Holochip Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Modified by Simone - 2026 */

#pragma once

#include <chrono>
#include <string>

namespace m1
{
	// Forward declaration
	class SceneObject;

	/**
	 * @brief Base class for all components in the engine.
	 *
	 * Components are the building blocks of the scene object-component system.
	 * Each component encapsulates a specific behavior or property.
	 *
	 * This class implements the component system as described in the Engine_Architecture chapter:
	 * https://github.com/KhronosGroup/Vulkan-Tutorial/blob/master/en/Building_a_Simple_Engine/Engine_Architecture/03_component_systems.adoc
	 */
	class Component
	{
		// TODO static field not thread safe
	private:
		static size_t nextTypeID;

	public:
		/**
		 * @brief Generates a unique type ID for components.
		 *
		 * This static method assigns a unique identifier to each component type
		 * within the engine's component-based system. The ID is created the first
		 * time the method is invoked for a specific type and remains consistent
		 * for subsequent calls for the same type during the application's runtime.
		 *
		 * The IDs are used to efficiently manage and query components within the
		 * engine.
		 *
		 * @return A unique identifier for the specific component type.
		 */
		template<typename T>
		static size_t GetTypeID()
		{
			static size_t typeID = nextTypeID++;
			return typeID;
		}

	protected:
		SceneObject* owner = nullptr;
		std::string name;
		bool active = true;

	public:
		/**
		 * @brief Constructor with optional name.
		 * @param componentName The name of the component.
		 */
		explicit Component(const std::string& componentName = "Component") : name(componentName) {}

		/**
		 * @brief Virtual destructor for proper cleanup.
		 */
		virtual ~Component() = default;

		[[nodiscard]] virtual size_t GetTypeID() const = 0; // = 0 make the class abstract

		/**
		 * @brief Initialize the component.
		 * Called when the component is added to a scene object.
		 */
		virtual void Initialize() {}

		/**
		 * @brief Update the component.
		 * Called every frame.
		 * @param deltaTime The time elapsed since the last frame.
		 */
		virtual void Update(std::chrono::milliseconds deltaTime) {}

		/**
		 * @brief Set the owner scene object of this component.
		 * @param sceneObject The scene object that owns this component.
		 */
		void SetOwner(SceneObject* sceneObject)
		{
			owner = sceneObject;
		}

		/**
		 * @brief Get the owner scene object of this component.
		 * @return The scene object that owns this component.
		 */
		[[nodiscard]] SceneObject* GetOwner() const
		{
			return owner;
		}

		/**
		 * @brief Get the name of the component.
		 * @return The name of the component.
		 */
		[[nodiscard]] const std::string& GetName() const
		{
			return name;
		}

		/**
		 * @brief Check if the component is active.
		 * @return True if the component is active, false otherwise.
		 */
		[[nodiscard]] bool IsActive() const
		{
			return active;
		}

		/**
		 * @brief Set the active state of the component.
		 * @param isActive The new active state.
		 */
		void SetActive(bool isActive)
		{
			active = isActive;
		}
	};
}
