import { NextResponse } from 'next/server';
import { SignJWT } from 'jose';

export async function POST(request: Request) {
  try {
    const JWT_SECRET = process.env.QUBIT_ENGINE_JWT_SECRET;
    if (!JWT_SECRET) {
      throw new Error('QUBIT_ENGINE_JWT_SECRET environment variable is not set');
    }
    const encodedSecret = new TextEncoder().encode(JWT_SECRET);
    const { userId, password } = await request.json();

    if (!userId) {
      return NextResponse.json({ error: 'Missing userId' }, { status: 400 });
    }

    const adminPassword = process.env.QUBIT_ENGINE_ADMIN_PASSWORD;
    if (!adminPassword || password !== adminPassword) {
      return NextResponse.json({ error: 'Unauthorized' }, { status: 401 });
    }

    const token = await new SignJWT()
      .setProtectedHeader({ alg: 'HS256' })
      .setSubject(userId)
      .setIssuer('qubit-engine')
      .setAudience('qubit-engine-api')
      .setIssuedAt()
      .setExpirationTime('24h')
      .sign(encodedSecret);

    return NextResponse.json({ token });
  } catch (error) {
    console.error('Error generating token:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
