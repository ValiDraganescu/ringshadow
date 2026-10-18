#include "AcHudText.h"

#include "AcHudStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/SlateRenderer.h"

#include "Simulation.h"
#include "Types.h"

#include <string>

// MARK: - FAcNames

FString FAcNames::Plural(const FString& Name)
{
	static const FString Vowels = TEXT("aeiou");
	if (Name.Len() >= 2 && Name.EndsWith(TEXT("y"), ESearchCase::CaseSensitive))
	{
		int32 Ignored;
		if (!Vowels.FindChar(Name[Name.Len() - 2], Ignored)) return Name.LeftChop(1) + TEXT("ies");
	}
	// Atlas -> Atlases.
	if (Name.EndsWith(TEXT("s"), ESearchCase::CaseSensitive)) return Name + TEXT("es");
	return Name + TEXT("s");
}

const TArray<FString>& FAcNames::Forms()
{
	static const TArray<FString> All = []
	{
		TArray<FString> Names;
		for (const ac::StructureKind K : ac::allCases<ac::StructureKind>())
		{
			Names.Add(UTF8_TO_TCHAR(ac::Simulation::title(K).c_str()));
		}
		for (const ac::UnitKind K : ac::allCases<ac::UnitKind>())
		{
			Names.Add(UTF8_TO_TCHAR(ac::Simulation::title(K).c_str()));
		}
		// FString's == ignores case, so no AddUnique here: "DERRICK" is a
		// form of its own beside "Derrick".
		TArray<FString> Out;
		auto Add = [&Out](const FString& F)
		{
			if (!Out.ContainsByPredicate([&F](const FString& G) { return G.Equals(F, ESearchCase::CaseSensitive); })) Out.Add(F);
		};
		for (const FString& N : Names)
		{
			for (const FString& F : {N, Plural(N)})
			{
				Add(F);
				Add(F.ToUpper());
			}
		}
		Out.StableSort([](const FString& A, const FString& B) { return A.Len() > B.Len(); });
		return Out;
	}();
	return All;
}

namespace
{
	/// A word character as ICU's `\b` sees it.
	bool IsWord(const TCHAR C)
	{
		return FChar::IsAlnum(C) || C == TEXT('_');
	}
}

const TArray<FAcTextRun>& FAcNames::Ranges(const FString& Text)
{
	// Keyed case-sensitively ("RANGER" and "Ranger" are different texts).
	struct FCaseSensitiveKeys : TDefaultMapKeyFuncs<FString, TArray<FAcTextRun>, false>
	{
		static bool Matches(const FString& A, const FString& B) { return A.Equals(B, ESearchCase::CaseSensitive); }
		static uint32 GetKeyHash(const FString& K) { return FCrc::StrCrc32(*K); }
	};
	static TMap<FString, TArray<FAcTextRun>, FDefaultSetAllocator, FCaseSensitiveKeys> Cache;
	if (const TArray<FAcTextRun>* Hit = Cache.Find(Text)) return *Hit;
	if (Cache.Num() > 4096) Cache.Reset();

	TArray<FAcTextRun> Found;
	const TArray<FString>& All = Forms();
	const int32 N = Text.Len();
	for (int32 I = 0; I < N;)
	{
		if (!IsWord(Text[I]) || (I > 0 && IsWord(Text[I - 1])))
		{
			++I;
			continue;
		}
		int32 Matched = 0;
		for (const FString& F : All)
		{
			const int32 L = F.Len();
			if (I + L > N || FCString::Strncmp(*Text + I, *F, L) != 0) continue;
			if (I + L < N && IsWord(Text[I + L])) continue;
			Matched = L;
			// A possessive's "'s" goes with its name.
			if (I + L + 1 < N && (Text[I + L] == TEXT('\'') || Text[I + L] == TEXT('\x2019'))
				&& (Text[I + L + 1] == TEXT('s') || Text[I + L + 1] == TEXT('S'))
				&& (I + L + 2 >= N || !IsWord(Text[I + L + 2])))
			{
				Matched += 2;
			}
			break;
		}
		if (Matched > 0)
		{
			Found.Add({I, Matched, true});
			I += Matched;
		}
		else
		{
			++I;
		}
	}
	return Cache.Add(Text, MoveTemp(Found));
}

TArray<FAcTextRun> FAcNames::Runs(const FString& Text)
{
	TArray<FAcTextRun> Out;
	int32 At = 0;
	for (const FAcTextRun& R : Ranges(Text))
	{
		if (R.Start > At) Out.Add({At, R.Start - At, false});
		Out.Add(R);
		At = R.Start + R.Len;
	}
	if (At < Text.Len() || Out.IsEmpty()) Out.Add({At, Text.Len() - At, false});
	return Out;
}

// MARK: - FAcHudText

FAcHudText::FAcHudText()
	: Accent(FAcHudStyle::NameAccent())
{
}

FAcHudText::FAcHudText(const FString& InText, const FSlateFontInfo& InFont, const FLinearColor& InColor, const EAcHAlign H,
	const EAcVAlign V)
	: Text(InText), Font(InFont), Color(InColor), Accent(FAcHudStyle::NameAccent()), HAlign(H), VAlign(V)
{
}

namespace
{
	TSharedRef<FSlateFontMeasure> Measurer()
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	}

	/// The measures a label is painted with, kept per text and font (docs/
	/// perf.md, client round 4): a kerned label placed each letter at the
	/// measured width of the text before it, every paint (n prefixes of a
	/// text of n letters, twice with the shadow). Filled as asked for, so a
	/// text that changes every frame (a timer) costs what it did.
	struct FMeasureKey
	{
		FString Text;
		FSlateFontInfo Font;
		bool operator==(const FMeasureKey& O) const { return Text.Equals(O.Text, ESearchCase::CaseSensitive) && Font == O.Font; }
		friend uint32 GetTypeHash(const FMeasureKey& K) { return HashCombine(GetTypeHash(K.Text), GetTypeHash(K.Font)); }
	};
	struct FMeasures
	{
		/// Prefix widths (`Measure(Text, 0, I)`), the letters' sizes; NaN: not yet.
		TArray<float> Prefix;
		TArray<FVector2f> Letter;
	};
	TAutoConsoleVariable<int32> CVarKeepMeasures(TEXT("ac.HudKeepMeasures"), 1,
		TEXT("HUD labels keep their measures per text and font (1), or measure every paint (0, to compare)."));

	FMeasures* MeasuresOf(const FString& Text, const FSlateFontInfo& Font)
	{
		if (CVarKeepMeasures.GetValueOnGameThread() == 0 || !IsInGameThread()) return nullptr;
		static TMap<FMeasureKey, FMeasures> Kept;
		if (Kept.Num() > 2048) Kept.Reset();
		FMeasures& M = Kept.FindOrAdd(FMeasureKey{Text, Font});
		if (M.Prefix.Num() != Text.Len() + 1)
		{
			M.Prefix.Init(NAN, Text.Len() + 1);
			M.Letter.Init(FVector2f(NAN, NAN), Text.Len());
		}
		return &M;
	}
}

FVector2f FAcHudText::Measure() const
{
	if (!FSlateApplication::IsInitialized()) return FVector2f::ZeroVector;
	const FVector2D S = Measurer()->Measure(Text, Font);
	// AppKit counts the kern after the last letter too (NSAttributedString.size).
	if (Kern != 0.0f) return FVector2f((float)S.X + Kern * Text.Len(), (float)S.Y);
	return FVector2f(S);
}

void FAcHudText::Paint(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry, FVector2f At,
	const float Opacity) const
{
	if (Text.IsEmpty() || Opacity <= 0.0f || !FSlateApplication::IsInitialized()) return;
	const TSharedRef<FSlateFontMeasure> M = Measurer();
	const FVector2f Size(Measure());
	// The em in points is the Swift size (Slate sizes are 96 dpi points).
	const float Em = Font.Size * 96.0f / 72.0f;
	const float Cap = Em * FAcHudStyle::CapHeight;
	const float LineHeight = (float)M->GetMaxCharacterHeight(Font);
	const float Descent = -(float)M->GetBaseline(Font);  // GetBaseline is the (negative) descender
	const float BaselineFromTop = LineHeight - Descent;

	float Baseline = At.Y;
	switch (VAlign)
	{
	case EAcVAlign::Baseline: break;
	case EAcVAlign::Center: Baseline = At.Y + Cap * 0.5f; break;
	case EAcVAlign::Top: Baseline = At.Y + Cap; break;
	}
	float Left = At.X;
	if (HAlign == EAcHAlign::Center) Left -= Size.X * 0.5f;
	else if (HAlign == EAcHAlign::Right) Left -= Size.X;
	const FVector2f Origin(Left, Baseline - BaselineFromTop);

	FMeasures* Kept = MeasuresOf(Text, Font);
	auto Prefix = [&](const int32 I) -> float
	{
		if (Kept && !FMath::IsNaN(Kept->Prefix[I])) return Kept->Prefix[I];
		const float W = (float)M->Measure(Text, 0, I, Font, false).X;
		if (Kept) Kept->Prefix[I] = W;
		return W;
	};
	const bool bMark = Accent.A > 0.0f;
	const TArray<FAcTextRun> Runs = bMark ? FAcNames::Runs(Text) : TArray<FAcTextRun>{{0, Text.Len(), false}};
	auto Draw = [&](const FVector2f Offset, const FLinearColor& Plain, const FLinearColor& Name, const int32 L)
	{
		for (const FAcTextRun& R : Runs)
		{
			if (R.Len <= 0) continue;
			FLinearColor C = R.bName ? Name : Plain;
			C.A *= Opacity;
			if (Kern != 0.0f)
			{
				// Letter by letter: each at the unkerned width of the text
				// before it (pair kerning kept) plus the spacing so far.
				for (int32 I = R.Start; I < R.Start + R.Len; ++I)
				{
					const float X = I > 0 ? Prefix(I) + Kern * I : 0.0f;
					const FString Letter = Text.Mid(I, 1);
					FVector2f Size = Kept ? Kept->Letter[I] : FVector2f(NAN, NAN);
					if (FMath::IsNaN(Size.X))
					{
						Size = FVector2f(M->Measure(Letter, Font));
						if (Kept) Kept->Letter[I] = Size;
					}
					FSlateDrawElement::MakeText(Out, L,
						Geometry.ToPaintGeometry(FVector2D(Size), FSlateLayoutTransform(FVector2D(Origin + Offset + FVector2f(X, 0)))),
						Letter, Font, ESlateDrawEffect::None, C);
				}
				continue;
			}
			const float X = R.Start > 0 ? Prefix(R.Start) : 0.0f;
			const FString Piece = Text.Mid(R.Start, R.Len);
			const FVector2f PieceSize(M->Measure(Piece, Font));
			FSlateDrawElement::MakeText(Out, L,
				Geometry.ToPaintGeometry(FVector2D(PieceSize), FSlateLayoutTransform(FVector2D(Origin + Offset + FVector2f(X, 0)))),
				Piece, Font, ESlateDrawEffect::None, C);
		}
	};
	if (ShadowColor.A > 0.0f) Draw(ShadowOffset, ShadowColor, ShadowColor, Layer);
	Draw(FVector2f::ZeroVector, Color, bMark ? Accent : Color, Layer + 1);
}
