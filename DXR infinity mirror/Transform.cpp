#include "Transform.h"

Transform::Transform()
{
	scale = { 1.0f, 1.0f, 1.0f };
	DirectX::XMStoreFloat4x4(&rotationMatrix, DirectX::XMMatrixRotationRollPitchYaw(0, 0, 0));
	position = { 0.0f, 0.0f, 0.0f };
	ConfirmChanges();
}

DirectX::XMFLOAT4X4 Transform::TransformMatrix()
{
	if (p_matrixUpToDate) return p_transformMatrix;

	DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);

	DirectX::XMMATRIX rotation = DirectX::XMLoadFloat4x4(&rotationMatrix);

	DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(position.x, position.y, position.z);

	DirectX::XMStoreFloat4x4(&p_transformMatrix, DirectX::XMMatrixTranspose(scaling * rotation * translation));

	p_matrixUpToDate = true;

	return p_transformMatrix;
}

DirectX::XMFLOAT4X4 Transform::InverseTransformMatrix()
{
	if (p_inverseUpToDate) return p_inverseTransformMatrix;

	DirectX::XMMATRIX invScaling = DirectX::XMMatrixScaling((scale.x != 0) ? 1.0f / scale.x : 0.0f, (scale.y != 0) ? 1.0f / scale.y : 0.0f, (scale.z != 0) ? 1.0f / scale.z : 0.0f);

	DirectX::XMMATRIX invRotation = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&rotationMatrix));

	DirectX::XMMATRIX invTranslation = DirectX::XMMatrixTranslation(-position.x, -position.y, -position.z);

	DirectX::XMStoreFloat4x4(&p_inverseTransformMatrix, DirectX::XMMatrixTranspose(invScaling * invRotation * invTranslation));

	p_inverseUpToDate = true;

	return p_inverseTransformMatrix;
}

void Transform::ConfirmChanges()
{
	p_matrixUpToDate = false;
	p_inverseUpToDate = false;
}

void Scale(Transform* transform, const std::array<float, 3>& scaling, Transformation_Orientation transformationOrientation, Transformation_Mode transformationMode, bool confirmChange)
{
	DirectX::XMFLOAT3 newScale = DirectX::XMFLOAT3(scaling[0], scaling[1], scaling[2]);

	if (transformationOrientation == Transformation_Orientation_Local)
	{

		DirectX::XMVECTOR scaleVector = DirectX::XMLoadFloat3(&newScale);
		DirectX::XMMATRIX rotationTransform = DirectX::XMLoadFloat4x4(&transform->rotationMatrix);
		scaleVector = DirectX::XMVector3Transform(scaleVector, rotationTransform);

		DirectX::XMFLOAT3 scaleTransformed;

		DirectX::XMStoreFloat3(&scaleTransformed, scaleVector);

		if (newScale.x * scaleTransformed.x < 0)
		{
			scaleTransformed.x *= -1;
		}
		if (newScale.y * scaleTransformed.y < 0)
		{
			scaleTransformed.y *= -1;
		}
		if (newScale.z * scaleTransformed.z < 0)
		{
			scaleTransformed.z *= -1;
		}

		newScale = scaleTransformed;
	}

	if (transformationMode == Transformation_Mode_Replace)
	{
		transform->scale = newScale;
	}
	else
	{
		transform->scale.x += newScale.x;
		transform->scale.y += newScale.y;
		transform->scale.z += newScale.z;
	}

	if (confirmChange)
		transform->ConfirmChanges();
}

void Rotate(Transform* transform, const std::array<float, 3>& rotation, Transformation_Orientation transformationOrientation, Transformation_Mode transformationMode, Rotaion_Unit rotationUnit, bool confirmChange)
{
	DirectX::XMMATRIX currentTransform = DirectX::XMLoadFloat4x4(&transform->rotationMatrix);

	DirectX::XMMATRIX transformation = DirectX::XMMatrixRotationRollPitchYaw(rotation[0] * rotationUnit, rotation[1] * rotationUnit, rotation[2] * rotationUnit);

	if (transformationOrientation == Transformation_Orientation_Local)
	{
		transformation = DirectX::XMMatrixTranspose(currentTransform) * transformation * currentTransform;
	}

	if (transformationMode == Transformation_Mode_Replace)
	{
		currentTransform = DirectX::XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f);
	}

	currentTransform *= transformation;

	DirectX::XMStoreFloat4x4(&transform->rotationMatrix, currentTransform);

	if (confirmChange)
		transform->ConfirmChanges();
}

void Rotate(Transform* transform, DirectX::XMFLOAT4X4& rotation, Transformation_Orientation transformationOrientation, Transformation_Mode transformationMode, bool confirmChange)
{
	DirectX::XMMATRIX currentTransform = DirectX::XMLoadFloat4x4(&transform->rotationMatrix);

	DirectX::XMMATRIX transformation = DirectX::XMLoadFloat4x4(&rotation);

	if (transformationOrientation == Transformation_Orientation_Local)
	{
		transformation = DirectX::XMMatrixTranspose(currentTransform) * transformation * currentTransform;
	}

	if (transformationMode == Transformation_Mode_Replace)
	{
		currentTransform = DirectX::XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f);
	}

	currentTransform *= transformation;

	DirectX::XMStoreFloat4x4(&transform->rotationMatrix, currentTransform);

	if (confirmChange)
		transform->ConfirmChanges();
}

void Translate(Transform* transform, const std::array<float, 3>& translation, Transformation_Orientation transformationOrientation, Transformation_Mode transformationMode, bool confirmChange)
{
	DirectX::XMFLOAT3 newTranslation = DirectX::XMFLOAT3(translation[0], translation[1], translation[2]);

	if (transformationOrientation == Transformation_Orientation_Local)
	{
		DirectX::XMVECTOR translationVector = DirectX::XMLoadFloat3(&newTranslation);
		DirectX::XMMATRIX rotationTransform = DirectX::XMLoadFloat4x4(&transform->rotationMatrix);
		translationVector = DirectX::XMVector3Transform(translationVector, rotationTransform);

		DirectX::XMStoreFloat3(&newTranslation, translationVector);
	}

	if (transformationMode == Transformation_Mode_Replace)
	{
		transform->position = newTranslation;
	}
	else
	{
		transform->position.x += newTranslation.x;
		transform->position.y += newTranslation.y;
		transform->position.z += newTranslation.z;
	}

	if (confirmChange)
		transform->ConfirmChanges();
}
