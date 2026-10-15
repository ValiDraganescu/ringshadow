// A drawing (FAcArtList) baked once into a texture: the console's art
// (faces, frames, plates) is drawn every frame but changes only with the
// layout, so it is drawn when the layout changes and the texture after that.
//
// Slate draws the list into a render target (in its gamma space, as the HUD
// on screen), which is read back, without waiting for the GPU, into an sRGB
// texture, as the Core Graphics art was: drawn smaller (2 pixels a point on
// a 1× screen) it is filtered in linear light, so its thin lit lines keep
// their weight. On a 2× screen, where the art is drawn 1:1, the target is
// drawn and nothing read back. Else the copy is read once the GPU is calm (on
// Metal a read waits for the whole GPU: a session's first seconds, a few
// seconds); until then the target itself is drawn, premultiplied and as it is
// (`Effects`, `Tint`).
#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/StrongObjectPtr.h"

#include "AcCab.h"

class FAcArtList;
struct FSlateBrush;
enum class ESlateDrawEffect : uint8;

class AUTOCRAFT_API FAcBakedArt
{
public:
	/// Draws the part of `List` in `Area` (Swift points, y up) at the list's scale.
	void Bake(const FAcArtList& List, const FAcRect& Area);
	void Reset();
	/// The art's brush, `Area` points in size; null before a bake. Draw it
	/// with `Effects()` and `Tint()` (FAcConsolePaint::Baked does).
	const FSlateBrush* Get() const;
	ESlateDrawEffect Effects() const;
	FLinearColor Tint(float Opacity = 1.0f) const;
	FVector2D Size() const;

	/// Every frame (a core ticker): asks for one waiting copy back once the
	/// GPU is calm, whether or not its art is drawn now (K3: a plate baked
	/// ahead was read only at its first sight, the crossing it was baked for).
	static void AskAll();

private:
	struct FPending;
	/// Takes the texture once the copy is back.
	void Poll() const;
	/// Asks for `P`'s copy back if the GPU is calm (or it waited long enough).
	static bool Ask(const TSharedPtr<FPending, ESPMode::ThreadSafe>& P);
	static TArray<TWeakPtr<FPending, ESPMode::ThreadSafe>>& Waiting();

	mutable TStrongObjectPtr<UTextureRenderTarget2D> Target;
	mutable TStrongObjectPtr<UTexture2D> Texture;
	mutable TSharedPtr<FSlateBrush> Brush;
	mutable TSharedPtr<FPending, ESPMode::ThreadSafe> Pending;
};
