#pragma once
#include <DirectXMath.h>
#include <array>

#define OBJECT_TRANSFORM_SPACE_LOCAL true
#define OBJECT_TRANSFORM_SPACE_GLOBAL false

#define PI 3.141592653589793238462643383279502884

#define OBJECT_ROTATION_UNIT_DEGREES PI / 180.0f
#define OBJECT_ROTATION_UINT_RADIANS 1.0f

#define OBJECT_TRANSFORM_REPLACE true
#define OBJECT_TRANSFORM_APPEND false

enum Rotaion_Unit
{
	Rotation_Unit_Degrees,
	Rotation_Unit_Radians
};

enum Transformation_Mode
{
	Transformation_Mode_Replace,
	Transformation_Mode_Append
};

enum Transformation_Orientation
{
	Transformation_Orientation_Local,
	Transformation_Orientation_Global
};

class Transform
{
public:
	Transform();

	DirectX::XMFLOAT4X4 TransformMatrix();
	DirectX::XMFLOAT4X4 InverseTransformMatrix();

	DirectX::XMFLOAT3 scale;
	DirectX::XMFLOAT4X4 rotationMatrix;
	DirectX::XMFLOAT3 position;

	void ConfirmChanges();

private:
	bool p_matrixUpToDate = false;
	bool p_inverseUpToDate = false;
	
	DirectX::XMFLOAT4X4 p_transformMatrix;
	DirectX::XMFLOAT4X4 p_inverseTransformMatrix;
};

void Scale(Transform* transform, const std::array<float, 3>& scaling, Transformation_Orientation transformationOrientation, Transformation_Mode transformationMode, bool confirmChange = true);

void Rotate(Transform* transform, const std::array<float, 3>& rotation, Transformation_Orientation transformationOrientation, Transformation_Mode transformationMode, Rotaion_Unit rotationUnit, bool confirmChange = true);
void Rotate(Transform* transform, DirectX::XMFLOAT4X4& rotation, Transformation_Orientation transformationOrientation, Transformation_Mode transformationMode, bool confirmChange = true);

void Translate(Transform* transform, const std::array<float, 3>& translation, Transformation_Orientation transformationOrientation, Transformation_Mode transformationMode, bool confirmChange = true);
