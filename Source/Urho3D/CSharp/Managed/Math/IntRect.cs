using System;
using System.Runtime.InteropServices;

namespace Urho3DNet
{
    [StructLayout(LayoutKind.Sequential)]
    public struct IntRect : IEquatable<IntRect>
    {
        /// Construct from coordinates.
        public IntRect(int left=0, int top=0, int right=0, int bottom=0)
        {
            Left = left;
            Top = top;
            Right = right;
            Bottom = bottom;
        }

        /// Construct from minimum and maximum vectors.
        public IntRect(in IntVector2 min, in IntVector2 max)
        {
            Left = min.X;
            Top = min.Y;
            Right = max.X;
            Bottom = max.Y;
        }

        /// Construct from an int array.
        public unsafe IntRect(int* data)
        {
            Left = data[0];
            Top = data[1];
            Right = data[2];
            Bottom = data[3];
        }

        /// Add two rects together.
        public static IntRect operator +(in IntRect lhs, in IntRect rhs)
        {
            return new IntRect(lhs.Left + rhs.Left, lhs.Top + rhs.Top, lhs.Right + rhs.Right, lhs.Bottom + rhs.Bottom);
        }

        /// Subtract rhs from lhs.
        public static IntRect operator -(in IntRect lhs, in IntRect rhs)
        {
            return new IntRect(lhs.Left - rhs.Left, lhs.Top - rhs.Top, lhs.Right - rhs.Right, lhs.Bottom - rhs.Bottom);
        }

        /// Divide lhs from rhs.
        public static IntRect operator /(in IntRect lhs, in IntRect rhs)
        {
            return new IntRect(lhs.Left / rhs.Left, lhs.Top / rhs.Top, lhs.Right / rhs.Right, lhs.Bottom / rhs.Bottom);
        }

        /// Multiply lhs by rhs.
        public static IntRect operator *(in IntRect lhs, in IntRect rhs)
        {
            return new IntRect(lhs.Left * rhs.Left, lhs.Top * rhs.Top, lhs.Right * rhs.Right, lhs.Bottom * rhs.Bottom);
        }

        /// Test for inequality with another rect.
        public static bool operator !=(in IntRect lhs, in IntRect rhs)
        {
            return !lhs.Equals(rhs);
        }

        /// Test for equality with another rect.
        public static bool operator ==(in IntRect lhs, in IntRect rhs)
        {
            return lhs.Equals(rhs);
        }

        /// Return size.
        public IntVector2 Size => new IntVector2(Width, Height);
        /// Return width.
        public int Width => Right - Left;
        /// Return height.
        public int Height => Bottom - Top;

                /// Test whether a point is inside.
        Intersection IsInside(in IntVector2 point)
        {
            if (point.X < Left || point.Y < Top || point.X >= Right || point.Y >= Bottom)
                return Intersection.Outside;
            else
                return Intersection.Inside;
        }

        /// Clip with another rect.  Since IntRect does not have an undefined state
        /// like Rect, return (0, 0, 0, 0) if the result is empty.
        void Clip(in IntRect rect)
        {
            if (rect.Left > Left)
                Left = rect.Left;
            if (rect.Right < Right)
                Right = rect.Right;
            if (rect.Top > Top)
                Top = rect.Top;
            if (rect.Bottom < Bottom)
                Bottom = rect.Bottom;

            if (Left >= Right || Top >= Bottom)
                Clear();
        }

        /// Merge a rect.  If this rect was empty, become the other rect.  If the
        /// other rect is empty, do nothing.
        void Merge(in IntRect rect)
        {
            if (Width <= 0 || Height <= 0)
            {
                Left = rect.Left;
                Top = rect.Top;
                Right = rect.Right;
                Bottom = rect.Bottom;
            }
            else if (rect.Width > 0 && rect.Height > 0)
            {
                if (rect.Left < Left)
                    Left = rect.Left;
                if (rect.Top < Top)
                    Top = rect.Top;
                if (rect.Right > Right)
                    Right = rect.Right;
                if (rect.Bottom > Bottom)
                    Bottom = rect.Bottom;
            }
        }

        /// Clear to undefined state.
        public void Clear()
        {
            Left = Right = Top = Bottom = 0;
        }

        /// Return as string.
        public override string ToString()
        {
            return $"{Left} {Top} {Right} {Bottom}";
        }

        /// Return left-top corner position.
        public IntVector2 Min => new IntVector2(Left, Top);
        /// Return right-bottom corner position.
        public IntVector2 Max => new IntVector2(Right, Bottom);
        /// Left coordinate.
        public int Left;
        /// Top coordinate.
        public int Top;
        /// Right coordinate.
        public int Right;
        /// Bottom coordinate.
        public int Bottom;

        /// Zero-sized rect.
        public static readonly IntRect Zero = new IntRect(0, 0, 0, 0);

        /// Test for equality with another rect.
        public bool Equals(IntRect other)
        {
            return Left == other.Left && Top == other.Top && Right == other.Right && Bottom == other.Bottom;
        }

        /// Test for equality with another rect.
        public override bool Equals(object obj)
        {
            if (ReferenceEquals(null, obj)) return false;
            return obj is IntRect && Equals((IntRect) obj);
        }

        /// Returns hash code.
        public override int GetHashCode()
        {
            unchecked
            {
                var hashCode = Left;
                hashCode = (hashCode * 397) ^ Top;
                hashCode = (hashCode * 397) ^ Right;
                hashCode = (hashCode * 397) ^ Bottom;
                return hashCode;
            }
        }
    }
}